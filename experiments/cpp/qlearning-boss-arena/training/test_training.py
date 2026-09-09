import copy
import random
import tempfile
import unittest
import subprocess
from pathlib import Path
import numpy as np
import torch
from algorithms import Agent, distribution, tensor
from environment import Environment, table_state, ROOT
from train import defaults, evaluate, run, validate


class TrainingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        torch.set_num_threads(1)

    def config(self, mode):
        return defaults() | dict(mode=mode, episodes=4, eval_every=2, eval_games=2, batch=16, rollout=32)

    def test_cpp_environment_deterministic_and_masked(self):
        env = Environment()
        def trajectory():
            x, mask = env.reset(123)
            items = []
            for _ in range(30):
                action = int(np.flatnonzero(mask)[-1])
                x, mask, r, done, seconds, win, loss = env.step(action)
                self.assertTrue(np.isfinite(x).all())
                self.assertGreater(seconds, 0)
                self.assertTrue(0 <= table_state(x) < 96)
                items.append((x.tolist(), mask.tolist(), r, done))
                if done:
                    break
            return items
        self.assertEqual(trajectory(), trajectory())
        env.reset(123)
        env.step(3)
        with self.assertRaises(RuntimeError):
            env.step(3)

    def test_noisy_kiter_reproducible_by_seed(self):
        env = Environment(0)
        def trajectory(seed):
            env.reset(seed)
            items = []
            for _ in range(20):
                x, _, reward, done, _, _, _ = env.step(env.baseline_action())
                items.append((x.tolist(), reward, done))
                if done:
                    break
            return items
        try:
            env.set_bot(3)
            first = trajectory(123)
            self.assertEqual(first, trajectory(123))
            self.assertNotEqual(first, trajectory(124))
        finally:
            env.set_bot(4)

    def test_invalid_configuration(self):
        for change in (dict(lr=-1), dict(gamma=2), dict(batch=0), dict(epsilon_min=.9), dict(mode="FAKE"), dict(sac_alpha=float("nan"))):
            with self.assertRaises(ValueError):
                validate(defaults() | change)

    def test_bot_profiles_baseline_and_combat_metrics(self):
        env = Environment(0)
        try:
            for bot in range(5):
                env.set_bot(bot)
                env.reset(123)
                while True:
                    _, _, _, done, _, win, _ = env.step(env.baseline_action())
                    if done:
                        break
                stats = env.stats()
                self.assertTrue(np.isfinite(stats).all())
                self.assertGreater(stats[2], 0)
                self.assertGreater(stats[4:].sum(), 0)
                self.assertLessEqual(stats[3], stats[2])
                if bot == 0:
                    self.assertTrue(win)
                    self.assertEqual(stats[0], 100)
                    self.assertEqual(stats[1], 0)
                env.reset(123)
                self.assertTrue((env.stats() == 0).all())
        finally:
            env.set_bot(4)

    def test_categorical_masks_and_entropy(self):
        mask = tensor([True, False, True, False, False, False], torch.bool)
        logits = torch.tensor([0., 100., 2., 100., 100., 100.], requires_grad=True)
        dist = distribution(logits, mask)
        self.assertEqual(float(dist.probs[1].detach()), 0)
        for _ in range(100):
            self.assertIn(int(dist.sample()), [0, 2])
        loss = -dist.entropy() + (dist.probs * dist.logits).sum()
        loss.backward()
        self.assertTrue(torch.isfinite(logits.grad).all())

    def test_dqn_terminal_bootstrap(self):
        agent = Agent(self.config("DQN"))
        for p in agent.policy.parameters():
            p.data.zero_()
        for p in agent.target.parameters():
            p.data.fill_(10.)
        x = np.zeros(24, dtype=np.float32)
        mask = np.ones(6, dtype=bool)
        # 종료 전이는 타깃 네트워크 값과 무관하게 보상 2만 사용한다.
        agent.replay.extend([(x, mask, 0, 2., x, mask, True, .96)] * 16)
        agent.update_replay()
        self.assertAlmostEqual(agent.loss, 1.5, places=5)

    def test_all_algorithms_update_export_restore(self):
        env = Environment()
        with tempfile.TemporaryDirectory(prefix="arena-rl-test-") as folder:
            for mode in ("QTABLE", "DQN", "PPO", "SAC"):
                with self.subTest(mode=mode):
                    torch.manual_seed(5)
                    agent = Agent(self.config(mode))
                    before = [p.detach().clone() for p in agent.policy.parameters()]
                    x, mask = env.reset(5)
                    for step in range(180):
                        a = agent.act(x, mask, True)
                        self.assertTrue(mask[a])
                        nx, nm, r, done, seconds, _, _ = env.step(a)
                        agent.observe(x, mask, a, r, nx, nm, done, seconds)
                        x, mask = env.reset(step + 100) if done else (nx, nm)
                    if mode == "PPO":
                        agent.update_ppo()
                    self.assertGreater(agent.updates, 0)
                    self.assertTrue(np.isfinite(agent.loss))
                    if mode != "QTABLE":
                        self.assertTrue(any(not torch.equal(old, new) for old, new in zip(before, agent.policy.parameters())))
                    else:
                        self.assertTrue(np.any(agent.table != 0))
                    path = Path(folder) / (mode + ".pt")
                    torch.save(agent.checkpoint(), path)
                    restored = Agent(self.config(mode))
                    restored.restore(torch.load(path, weights_only=True))
                    self.assertEqual(restored.updates, agent.updates)
                    self.assertEqual(restored.act(x, mask, False), agent.act(x, mask, False))
                    if mode != "QTABLE":
                        exported = Path(folder) / (mode + ".nn")
                        agent.export(exported)
                        for _ in range(5):
                            obs = np.random.uniform(-1, 1, 24).astype(np.float32)
                            py = agent.policy(tensor(obs)).detach().numpy()
                            cpp = env.cpp_forward(exported, obs)
                            np.testing.assert_allclose(py, cpp, rtol=2e-5, atol=2e-5)
                        score = evaluate(agent, Environment(1), 3)
                        completed = subprocess.run([str(ROOT / "build/Release/Containment.exe"), "--eval-model", str(exported), "3", "1000000"], capture_output=True, text=True, check=True)
                        fields = dict(item.split("=") for item in completed.stdout.split())
                        self.assertEqual(int(fields["wins"]), round(score[1] * 3))
                        self.assertAlmostEqual(float(fields["mean_reward"]), score[0], places=3)
                        bad = Path(folder) / "invalid.nn"
                        bad.write_text("ARENA_NN_V1 FAKE 24 64 6 0\n")
                        with self.assertRaises(RuntimeError):
                            env.cpp_forward(bad, x)

    def test_evaluation_freezes_learning_and_rng(self):
        agent = Agent(self.config("PPO"))
        before = copy.deepcopy(agent.checkpoint())
        py_rng, torch_rng = random.getstate(), torch.get_rng_state().clone()
        score = evaluate(agent, Environment(1), 3)
        self.assertEqual(score, evaluate(agent, Environment(1), 3))
        self.assertEqual(agent.updates, before["updates"])
        for key, value in agent.policy.state_dict().items():
            self.assertTrue(torch.equal(value, before["policy"][key]))
        self.assertEqual(random.getstate(), py_rng)
        self.assertTrue(torch.equal(torch.get_rng_state(), torch_rng))

    def test_run_metrics_best_latest_and_resume(self):
        with tempfile.TemporaryDirectory(prefix="arena-run-test-") as tmp:
            folder = Path(tmp) / "first"
            agent = run(self.config("PPO"), folder)
            self.assertEqual(len((folder / "metrics.tsv").read_text().splitlines()), 6)
            for name in ("best.pt", "latest.pt", "best.nn", "latest.nn", "config.json", "finished.json"):
                self.assertTrue((folder / name).is_file())
            next_agent = run(self.config("PPO") | dict(episodes=1), Path(tmp) / "resume", folder / "latest.pt")
            self.assertGreater(next_agent.updates, agent.updates)
            with self.assertRaises(ValueError):
                run(self.config("PPO"), folder)

    def test_stop_preserves_checkpoint(self):
        with tempfile.TemporaryDirectory(prefix="arena-stop-test-") as tmp:
            folder = Path(tmp)
            (folder / "STOP").touch()
            agent = run(self.config("DQN"), folder)
            self.assertEqual(agent.updates, 0)
            self.assertTrue((folder / "latest.pt").is_file())
            self.assertIn('"stopped": true', (folder / "finished.json").read_text())


if __name__ == "__main__":
    unittest.main(verbosity=2)
