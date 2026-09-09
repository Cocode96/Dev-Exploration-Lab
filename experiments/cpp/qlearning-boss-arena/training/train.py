"""ImGui가 실행하는 CPU 학습 프로세스. 학습/평가 환경과 난수 스트림을 분리한다."""
import argparse
import json
import random
from pathlib import Path
import traceback
import math
import numpy as np
import torch
from algorithms import Agent
from environment import Environment


def defaults():
    return dict(mode="DQN", episodes=500, lr=.0003, table_lr=.16, gamma=.96,
                epsilon=.3, epsilon_min=.05, epsilon_decay=.995, batch=64,
                rollout=256, clip=.2, gae_lambda=.95, entropy=.01, sac_alpha=.2,
                eval_every=25, eval_games=20, seed=42, patience=0, bot=1)


def validate(c):
    if any(isinstance(value, (int, float)) and not math.isfinite(value) for value in c.values()):
        raise ValueError("Settings must be finite")
    if c["mode"] not in ("QTABLE", "DQN", "PPO", "SAC"):
        raise ValueError("Unknown mode")
    if c["bot"] not in range(5):
        raise ValueError("Unknown bot")
    for key in ("episodes", "eval_every", "eval_games", "batch", "rollout"):
        if c[key] < 1:
            raise ValueError(f"{key} must be positive")
    if not 0 < c["lr"] <= .1 or not 0 < c["table_lr"] <= 1 or not 0 < c["gamma"] <= 1:
        raise ValueError("Invalid learning rate or gamma")
    if not 0 <= c["epsilon_min"] <= c["epsilon"] <= 1 or not 0 < c["epsilon_decay"] <= 1:
        raise ValueError("Invalid exploration settings")
    if not 0 < c["clip"] < 1 or not 0 <= c["gae_lambda"] <= 1 or c["entropy"] < 0 or c["sac_alpha"] <= 0:
        raise ValueError("Invalid PPO/SAC settings")
    if c["seed"] < 0 or c["patience"] < 0:
        raise ValueError("Invalid seed or patience")


def save(agent, folder, name):
    checkpoint = folder / (name + ".pt")
    tmp = checkpoint.with_suffix(".pt.tmp")
    torch.save(agent.checkpoint(), tmp)
    tmp.replace(checkpoint)
    agent.export(folder / (name + (".qtable" if agent.mode == "QTABLE" else ".nn")))


def evaluate(agent, env, games, seed_base=1000000, bot=4):
    # 평가가 학습의 난수 순서를 바꾸지 않도록 보존한다.
    rng, nrng, trng = random.getstate(), np.random.get_state(), torch.get_rng_state()
    rewards, wins, losses = [], 0, 0
    env.set_bot(bot)
    try:
        for game in range(games):
            x, mask = env.reset(seed_base + game)
            total = 0.
            while True:
                action = agent.act(x, mask, False)
                x, mask, r, done, _, win, loss = env.step(action)
                total += r
                if done:
                    wins += win
                    losses += loss
                    rewards.append(total)
                    break
        return float(np.mean(rewards)), wins / games, losses / games
    finally:
        random.setstate(rng)
        np.random.set_state(nrng)
        torch.set_rng_state(trng)


def run(config, folder, resume=None):
    validate(config)
    folder = Path(folder)
    folder.mkdir(parents=True, exist_ok=True)
    if (folder / "metrics.tsv").exists():
        raise ValueError("Run already exists; use a new run folder")
    (folder / "config.json").write_text(json.dumps(config, indent=2), encoding="utf-8")
    random.seed(config["seed"])
    np.random.seed(config["seed"])
    torch.manual_seed(config["seed"])
    torch.set_num_threads(1)
    agent = Agent(config)
    if resume:
        agent.restore(torch.load(resume, map_location="cpu", weights_only=True))
    env, validation = Environment(0), Environment(1)
    env.set_bot(config["bot"])
    best_score, stale = None, 0
    history = []
    completed = 0
    stopped = False
    with (folder / "metrics.tsv").open("w", buffering=1, encoding="ascii") as log:
        log.write("episode\treward\tmean20\teval_reward\twin_rate\tloss_rate\tloss\tactor_loss\tentropy\tkl\tepsilon\tupdates\tdealt\ttaken\tattempts\thits\tapproach\torbit\tslash\tcharge\tpulse\tfan\n")

        def record(episode, reward, do_eval):
            nonlocal best_score, stale
            er, wr, lr = float("nan"), float("nan"), float("nan")
            if do_eval:
                er, wr, lr = evaluate(agent, validation, config["eval_games"], bot=config["bot"])
                score = (wr, er)
                if best_score is None or score > best_score:
                    best_score, stale = score, 0
                    save(agent, folder, "best")
                    (folder / "best.json").write_text(json.dumps(dict(episode=episode, win_rate=wr, reward=er)), encoding="utf-8")
                else:
                    stale += 1
                save(agent, folder, "latest")
            row = (episode, reward, float(np.mean(history[-20:])) if history else 0., er, wr, lr,
                   agent.loss, agent.actor_loss, agent.entropy, agent.kl,
                   agent.epsilon if agent.mode in ("QTABLE", "DQN") else float("nan"), agent.updates)
            stats = tuple(env.stats()) if episode else (0,) * 10
            log.write("\t".join(map(str, row + stats)) + "\n")
            log.flush()

        record(0, 0, True)  # 학습 전 모델도 같은 조건으로 평가한다.
        for episode in range(1, config["episodes"] + 1):
            if (folder / "STOP").exists():
                stopped = True
                break
            x, mask = env.reset(config["seed"] + episode)
            total = 0.
            while True:
                action = agent.act(x, mask, True)
                nx, nm, r, done, seconds, _, _ = env.step(action)
                agent.observe(x, mask, action, r, nx, nm, done, seconds)
                total += r
                x, mask = nx, nm
                if done:
                    break
            history.append(total)
            completed = episode
            if agent.mode in ("QTABLE", "DQN"):
                agent.epsilon = max(config["epsilon_min"], agent.epsilon * config["epsilon_decay"])
            do_eval = episode % config["eval_every"] == 0 or episode == config["episodes"]
            # PPO は評価前に現在の on-policy バッチを消費する。
            if do_eval and agent.mode == "PPO":
                agent.update_ppo()
            record(episode, total, do_eval)
            if config["patience"] and stale >= config["patience"]:
                stopped = True
                break
        if agent.mode == "PPO":
            agent.update_ppo()
        save(agent, folder, "latest")
    result = dict(episodes=completed, stopped=stopped, updates=agent.updates,
                  best_validation_win_rate=best_score[0], best_validation_reward=best_score[1])
    (folder / "finished.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps(result), flush=True)
    return agent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--run", required=True)
    parser.add_argument("--resume")
    for key, value in defaults().items():
        parser.add_argument("--" + key.replace("_", "-"), type=type(value), default=value)
    args = vars(parser.parse_args())
    folder, resume = args.pop("run"), args.pop("resume")
    try:
        run(args, folder, resume)
    except Exception:
        Path(folder).mkdir(parents=True, exist_ok=True)
        (Path(folder) / "error.txt").write_text(traceback.format_exc(), encoding="utf-8")
        raise


if __name__ == "__main__":
    main()
