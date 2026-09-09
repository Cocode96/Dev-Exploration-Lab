"""학습 없는 기준 보스와 저장 정책을 동일한 상대/시드에서 비교한다."""
import argparse
import json
import numpy as np
import torch
from algorithms import Agent
from environment import Environment

BOT_NAMES = ["target", "rookie", "rusher", "kiter", "legacy"]


def benchmark(games=100, checkpoint=None, seed_base=3000000):
    torch.set_num_threads(1)
    agent = None
    if checkpoint:
        data = torch.load(checkpoint, weights_only=True, map_location="cpu")
        agent = Agent(data["config"])
        agent.restore(data)
    env = Environment(1)
    reports = []
    for bot, name in enumerate(BOT_NAMES):
        env.set_bot(bot)
        wins = 0
        totals = np.zeros(10)
        for game in range(games):
            x, mask = env.reset(seed_base + game)
            while True:
                action = agent.act(x, mask, False) if agent else env.baseline_action()
                x, mask, _, done, _, win, _ = env.step(action)
                if done:
                    wins += win
                    totals += env.stats()
                    break
        choices = totals[4:]
        reports.append(dict(bot=name, wins=wins, games=games, dealt=round(totals[0]/games, 2),
                            taken=round(totals[1]/games, 2), hit_rate=round(totals[3]/max(1, totals[2]), 4),
                            action_ratios=(choices/max(1, choices.sum())).round(4).tolist()))
    env.set_bot(4)
    return reports


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--checkpoint")
    parser.add_argument("--games", type=int, default=100)
    args = parser.parse_args()
    if args.games < 1:
        parser.error("games must be positive")
    print(json.dumps(benchmark(args.games, args.checkpoint), indent=2))
