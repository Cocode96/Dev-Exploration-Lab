"""모델 선택에 사용하지 않은 별도 시드로 최종 점검한다. 학습/파일 변경 없음."""
import argparse
import json
import math
import torch
from algorithms import Agent
from environment import Environment
from train import evaluate


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("checkpoint")
    parser.add_argument("--games", type=int, default=100)
    parser.add_argument("--seed-base", type=int, default=2000000)
    parser.add_argument("--bot", type=int, choices=range(5))
    args = parser.parse_args()
    if args.games < 1 or args.seed_base < 0:
        parser.error("games must be positive; seed-base must be nonnegative")
    torch.set_num_threads(1)
    data = torch.load(args.checkpoint, map_location="cpu", weights_only=True)
    agent = Agent(data["config"])
    agent.restore(data)
    bot = data["config"].get("bot", 4) if args.bot is None else args.bot
    reward, win, loss = evaluate(agent, Environment(1), args.games, args.seed_base, bot=bot)
    z, n = 1.96, args.games
    center = (win + z*z/(2*n)) / (1 + z*z/n)
    half = z*math.sqrt(win*(1-win)/n + z*z/(4*n*n)) / (1 + z*z/n)
    print(json.dumps(dict(mode=agent.mode, bot=bot, games=n, seed_base=args.seed_base,
                         mean_reward=reward, win_rate=win, loss_rate=loss,
                         timeout_rate=1-win-loss, win_wilson95=[center-half, center+half]), indent=2))
