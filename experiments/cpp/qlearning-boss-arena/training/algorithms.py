"""DQN, PPO-Clip/GAE, discrete SAC. 모든 알고리즘은 같은 6개 FSM 행동을 사용한다."""
from collections import deque
from copy import deepcopy
import random
import numpy as np
import torch
from torch import nn
from torch.distributions import Categorical
from torch.nn import functional as F
from environment import OBS, ACTIONS, table_state


def network(outputs=ACTIONS):
    return nn.Sequential(nn.Linear(OBS, 64), nn.ReLU(), nn.Linear(64, 64), nn.ReLU(), nn.Linear(64, outputs))


def tensor(x, dtype=torch.float32):
    return torch.as_tensor(np.asarray(x), dtype=dtype)


def distribution(logits, mask):
    return Categorical(logits=logits.masked_fill(~mask, -1e9))


def optimize(optimizer, loss, parameters):
    if not torch.isfinite(loss):
        raise RuntimeError("Non-finite training loss")
    optimizer.zero_grad()
    loss.backward()
    nn.utils.clip_grad_norm_(parameters, 1.0, error_if_nonfinite=True)
    optimizer.step()


def soft_update(source, target, tau=.01):
    with torch.no_grad():
        for src, dst in zip(source.parameters(), target.parameters()):
            dst.lerp_(src, tau)


class Agent:
    def __init__(self, config):
        self.c = config
        self.mode = config["mode"]
        self.updates = 0
        self.loss = 0.
        self.actor_loss = 0.
        self.entropy = 0.
        self.kl = 0.
        self.epsilon = config["epsilon"]
        self.policy = network()
        self.optimizer = torch.optim.Adam(self.policy.parameters(), lr=config["lr"])
        self.replay = deque(maxlen=50000)
        self.rollout = []
        self.table = np.zeros((96, ACTIONS), dtype=np.float32)
        self.steps = 0
        if self.mode == "DQN":
            self.target = deepcopy(self.policy).requires_grad_(False)
        if self.mode == "PPO":
            self.value = network(1)
            self.value_optimizer = torch.optim.Adam(self.value.parameters(), lr=config["lr"])
        if self.mode == "SAC":
            self.q1, self.q2 = network(), network()
            self.target1, self.target2 = deepcopy(self.q1).requires_grad_(False), deepcopy(self.q2).requires_grad_(False)
            self.q_optimizer = torch.optim.Adam(list(self.q1.parameters()) + list(self.q2.parameters()), lr=config["lr"])

    @torch.no_grad()
    def act(self, x, mask, training):
        if self.mode in ("QTABLE", "DQN") and training and random.random() < self.epsilon:
            return int(random.choice(np.flatnonzero(mask)))
        if self.mode == "QTABLE":
            scores = self.table[table_state(x)]
            return int(np.argmax(np.where(mask, scores, -np.inf)))
        scores = self.policy(tensor(x))
        if self.mode in ("PPO", "SAC") and training:
            return int(distribution(scores, tensor(mask, torch.bool)).sample())
        return int(scores.masked_fill(~tensor(mask, torch.bool), -1e9).argmax())

    def observe(self, x, mask, action, reward, nx, nm, done, seconds):
        self.steps += 1
        discount = self.c["gamma"] ** seconds
        if self.mode == "QTABLE":
            s, ns = table_state(x), table_state(nx)
            target = reward + (0 if done else discount * self.table[ns][nm].max())
            error = target - self.table[s, action]
            self.table[s, action] += self.c["table_lr"] * error
            self.loss = float(error * error)
            self.updates += 1
        elif self.mode == "PPO":
            with torch.no_grad():
                dist = distribution(self.policy(tensor(x)), tensor(mask, torch.bool))
                logp = float(dist.log_prob(torch.tensor(action)))
                value = float(self.value(tensor(x)).squeeze())
                nv = float(self.value(tensor(nx)).squeeze())
            self.rollout.append((x, mask, action, reward, done, discount, logp, value, nv))
            if len(self.rollout) >= self.c["rollout"]:
                self.update_ppo()
        else:
            self.replay.append((x, mask, action, reward, nx, nm, done, discount))
            if len(self.replay) >= max(128, self.c["batch"]) and self.steps % 4 == 0:
                self.update_replay()

    def update_replay(self):
        batch = random.sample(self.replay, self.c["batch"])
        x, mask, a, r, nx, nm, done, discount = zip(*batch)
        x, nx, r, discount = map(tensor, (x, nx, r, discount))
        mask, nm = tensor(mask, torch.bool), tensor(nm, torch.bool)
        a, alive = tensor(a, torch.long), 1 - tensor(done)
        if self.mode == "DQN":
            with torch.no_grad():
                next_value = self.target(nx).masked_fill(~nm, -1e9).max(1).values
                target = r + discount * alive * next_value
            loss = F.smooth_l1_loss(self.policy(x).gather(1, a[:, None]).squeeze(1), target)
            optimize(self.optimizer, loss, self.policy.parameters())
            soft_update(self.policy, self.target)
        else:
            # 이산 SAC: 가능한 행동의 기대값을 정확히 합산한다.
            alpha = self.c["sac_alpha"]
            with torch.no_grad():
                next_dist = distribution(self.policy(nx), nm)
                next_q = torch.minimum(self.target1(nx), self.target2(nx))
                next_value = (next_dist.probs * (next_q - alpha * next_dist.logits)).sum(1)
                target = r + discount * alive * next_value
            q1 = self.q1(x).gather(1, a[:, None]).squeeze(1)
            q2 = self.q2(x).gather(1, a[:, None]).squeeze(1)
            loss = F.mse_loss(q1, target) + F.mse_loss(q2, target)
            optimize(self.q_optimizer, loss, list(self.q1.parameters()) + list(self.q2.parameters()))
            dist = distribution(self.policy(x), mask)
            with torch.no_grad():
                q = torch.minimum(self.q1(x), self.q2(x))
            actor_loss = (dist.probs * (alpha * dist.logits - q)).sum(1).mean()
            optimize(self.optimizer, actor_loss, self.policy.parameters())
            self.actor_loss = float(actor_loss.detach())
            self.entropy = float(dist.entropy().mean().detach())
            soft_update(self.q1, self.target1)
            soft_update(self.q2, self.target2)
        self.loss = float(loss.detach())
        self.updates += 1

    def update_ppo(self):
        if not self.rollout:
            return
        x, mask, a, r, done, discounts, old_logp, values, next_values = zip(*self.rollout)
        advantages = np.zeros(len(r), dtype=np.float32)
        gae = 0.
        for t in reversed(range(len(r))):
            alive = 1 - done[t]
            delta = r[t] + discounts[t] * alive * next_values[t] - values[t]
            gae = delta + discounts[t] * self.c["gae_lambda"] * alive * gae
            advantages[t] = gae
        returns = tensor(advantages + np.asarray(values))
        adv = tensor(advantages)
        adv = (adv - adv.mean()) / (adv.std(unbiased=False) + 1e-8)
        x, mask, a, old = tensor(x), tensor(mask, torch.bool), tensor(a, torch.long), tensor(old_logp)
        stop = False
        for _ in range(4):
            for ids in torch.randperm(len(r)).split(self.c["batch"]):
                dist = distribution(self.policy(x[ids]), mask[ids])
                log_ratio = dist.log_prob(a[ids]) - old[ids]
                ratio = log_ratio.exp()
                self.kl = float(((ratio - 1) - log_ratio).mean().detach())
                if self.kl > .03:
                    stop = True
                    break
                clipped = ratio.clamp(1 - self.c["clip"], 1 + self.c["clip"])
                actor_loss = -torch.minimum(ratio * adv[ids], clipped * adv[ids]).mean() - self.c["entropy"] * dist.entropy().mean()
                value_loss = .5 * F.mse_loss(self.value(x[ids]).squeeze(1), returns[ids])
                optimize(self.optimizer, actor_loss, self.policy.parameters())
                optimize(self.value_optimizer, value_loss, self.value.parameters())
                self.loss = float(value_loss.detach())
                self.actor_loss = float(actor_loss.detach())
                self.entropy = float(dist.entropy().mean().detach())
                self.updates += 1
            if stop:
                break
        self.rollout.clear()

    def checkpoint(self):
        data = {"version": 1, "config": self.c, "updates": self.updates, "steps": self.steps,
                "table": torch.from_numpy(self.table.copy()), "epsilon": self.epsilon}
        for name in ("policy", "optimizer", "target", "value", "value_optimizer", "q1", "q2", "target1", "target2", "q_optimizer"):
            if hasattr(self, name):
                data[name] = getattr(self, name).state_dict()
        return data

    def restore(self, data):
        if data["version"] != 1 or data["config"]["mode"] != self.mode:
            raise ValueError("Checkpoint algorithm mismatch")
        for name in ("policy", "optimizer", "target", "value", "value_optimizer", "q1", "q2", "target1", "target2", "q_optimizer"):
            if hasattr(self, name):
                getattr(self, name).load_state_dict(data[name])
        self.table = data["table"].numpy().copy()
        self.updates, self.steps, self.epsilon = data["updates"], data["steps"], data["epsilon"]
        # 이번 실행의 학습률을 적용한다. replay/진행 중 rollout은 새로 수집한다.
        for name in ("optimizer", "value_optimizer", "q_optimizer"):
            if hasattr(self, name):
                for group in getattr(self, name).param_groups:
                    group["lr"] = self.c["lr"]

    def export(self, path):
        temp = path.with_suffix(path.suffix + ".tmp")
        with temp.open("w", encoding="ascii") as f:
            if self.mode == "QTABLE":
                f.write(f"CONTAINMENT_Q1 {self.updates}\n")
                np.savetxt(f, self.table, fmt="%.9g")
            else:
                f.write(f"ARENA_NN_V1 {self.mode} {OBS} 64 {ACTIONS} {self.updates}\n")
                for layer in (self.policy[0], self.policy[2], self.policy[4]):
                    for p in (layer.weight, layer.bias):
                        np.savetxt(f, p.detach().numpy().reshape(1, -1), fmt="%.9g")
        temp.replace(path)
