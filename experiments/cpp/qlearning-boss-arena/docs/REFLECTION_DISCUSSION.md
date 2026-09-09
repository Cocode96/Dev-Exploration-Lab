# 보스 AI가 선회만 반복할 때, 모델보다 학습 환경을 먼저 검증해야 할까?

## 고찰의 출발점

> 사실 더 좋은 모델 이란게 모호한데 거기에 너무 집착했나봐 오히려 qtable이 학습하기 좋은 학습 환경을 만들어주는데 집중해보자

처음 목표는 C++ 탑뷰 게임에서 보스가 기본 공격과 스킬을 상황에 맞게 선택하게 만드는 것이었다. [FSM을 대체하지 않고 Q-learning으로 행동 선택을 보강하는 기존 고찰](https://github.com/Cocode96/Dev-Exploration-Lab/issues/20)을 실제 전투 실험으로 옮겼다.

Q-table에서 DQN/PPO/SAC까지 모드를 늘렸지만 보스가 잘 싸우지는 않았다. 직접 보니 선회만 반복하는 모습도 있었다. 더 복잡한 모델을 붙이기 전에, 내가 말하는 '더 좋다'의 기준과 학습 가능한 환경을 먼저 정했어야 한다고 생각했다.

## 구현한 것과 확인한 사실

- C++/DirectX 11 탑뷰 전투, 기본 공격과 스킬 3개, FSM 공격 예고/실행/후딜, 쿨다운 행동 마스킹.
- C++ 전투 DLL을 Python 학습에서 그대로 호출하고, 학습된 작은 신경망을 C++에서 추론한다. 전투를 Python으로 복제하지 않았다.
- ImGui 학습 모드/설정, 보상/평가 승률/손실 곡선, best/latest 체크포인트와 이름별 모델 보관.
- 이전 SAC 실행에서 375판의 best는 별도 시드 100판에 15승, 500판 latest는 2승이었다. 한 실행의 결과이지 SAC 자체의 우열을 증명하는 수치는 아니다.
- 선회 편중의 원인이 회피적 보상 최적화인지, 관측 부족인지, 공격 난도인지 아직 분리해서 증명하지 못했다. UI에서 선택한 모드와 로드한 모델이 다른 상태도 학습 성능과 구분해야 한다.

## 이번 결정: Q-table과 학습 환경부터

Q-table이 모든 문제에서 더 우수하다는 결론은 아니다. 관측/행동/보상/상대 난도를 통제하고 디버깅하기 쉬운 기준으로 먼저 사용하려는 결정이다. 신경망 모드와 기존 모델은 삭제하지 않고 비교 대상으로 보존한다.

1. 학습 없는 규칙 보스로 전투 자체가 성립하는지 확인한다.
2. 정지 표적, 느린 초보, 돌격, 거리 유지, 기존 고난도 봇을 분리한다.
3. 쉬운 상대부터 Q-table을 학습하고 상대별 평가를 별도로 수행한다.
4. 승률 외에 준 피해/받은 피해, 명중 기회 대비 적중, 행동 선택 비율을 기록한다.
5. 단순한 보상 조작으로 공격 버튼만 반복하게 만들지 않는다. 보상 변경은 현재 결과를 기준으로 한 변수씩 검증할 후속 실험이다.

## 환경을 나눈 첫 결과

2026-09-09, 평가 시드 3000000부터 각 100판. Q-table은 초보 봇 상대 500판 학습, 25판마다 별도 validation 20판으로 best 선택. 다른 상대는 이 학습에 포함하지 않았다.

| 상대 | 규칙 기준 보스 | Q-table best |
|---|---:|---:|
| 정지 표적, 사격 없음 | 100/100 | 100/100 |
| 느린 초보, 회피 없음 | 100/100 | 100/100 |
| 돌격형 | 100/100 | 48/100 |
| 거리 유지, 늦은 회피 | 39/100 | 1/100 |
| 기존 고난도 봇 | 7/100 | 0/100 |

초보 상대의 평균 받은 피해는 기준 보스 111.8, Q-table 133.55였다. 승률이 같아도 학습 보스가 더 잘 싸운다고 주장할 수 없다. Q-table의 초보 상대 선회 비율은 28.35%였다. 선회 자체를 무조건 나쁜 행동으로 규정할 수도 없다.

이 결과는 'Q-table이 신경망보다 낫다'가 아니라, **쉬운 조건에서 학습이 작동하는 것과 다른 상대에게 일반화되는 것은 다르다**는 출발점이다. 봇들은 난도뿐 아니라 이동/사격/회피 성향도 다르므로 단일 변수 인과 실험으로 해석하지 않는다.

## 포트폴리오로 남기고 싶은 점

알고리즘 이름의 개수보다 C++ 런타임과 학습 환경의 경계를 설계하고, 실제 플레이의 이상 행동을 관찰하고, 평가 기준을 다시 세운 과정을 남기고 싶다. 개인 실험에서 Codex를 코드 작성과 자동 검증에 활용했으며, 문제 설정과 플레이 관찰 및 방향 수정은 대화에서 직접 결정했다. 구현 원리를 설명하고 재현하는 것은 계속 검토해야 할 부분이다.

현재는 학습/추론 연결과 환경별 진단이 구현된 프로토타입이다. 사람에게 재미있는 적응형 보스, 여러 상대에게 일반화된 정책, 규칙 보스를 능가하는 효율은 아직 달성한 결과가 아니다. 추론 비용도 별도 프로파일링 전이므로 수치로 주장하지 않는다.

## 함께 논의하고 싶은 질문

**이처럼 공격을 못 하거나 특정 행동만 반복하는 보스를 개선할 때, 모델을 바꾸기 전에 상태 추상화/보상/상대 난도 중 어떤 순서와 기준으로 점검하는 것이 좋을까?**

다음 실험은 Q-table을 고정한 채 상대와 관측/보상을 한 번에 하나씩 바꾸고, 고정 시드의 기준 보스 및 여러 학습 seed와 비교하는 것이다. 난도 자동 승급이나 혼합 상대 학습은 아직 구현 완료로 기록하지 않는다.

코드 위치: `experiments/cpp/qlearning-boss-arena`. 환경 추가 로컬 커밋: `211715b`. 현재 브랜치 작업은 아직 push하지 않았으며 원격 코드 공개 완료를 뜻하지 않는다.

---

## English

### Reflection: define a learnable environment before chasing a better model

My original goal was a C++ top-down boss whose learned policy chooses attacks while an FSM executes telegraphs, active frames, and recovery. I expanded the experiment from Q-tables to DQN, PPO, and discrete SAC, but the boss still fought poorly and sometimes circled repeatedly. I had focused on a vaguely defined 'better model' before establishing a useful environment and evaluation baseline.

The prototype shares the C++ combat DLL with Python training and runs exported neural weights in C++. ImGui exposes training settings, reward/evaluation/loss curves, and separate best/latest checkpoints. In one earlier SAC run, the intermediate checkpoint won 15 of 100 held-out-seed games, whereas the final checkpoint won two. This is one run, not evidence that SAC is inherently worse. Reward avoidance, incomplete observations, combat difficulty, and UI model-selection mismatches remain distinct explanations to investigate.

I am now keeping Q-learning fixed while validating the environment. I added a rule-based boss and five opponent profiles: stationary target, slow rookie, rusher, kiter with delayed dodging, and the original difficult bot. On 100 seeds per profile, the baseline boss won 100/100/100/39/7 games respectively. A Q-table trained for 500 rookie episodes won 100/100/48/1/0 with its validation-selected best checkpoint. Against the rookie, the rule boss took 111.8 mean damage versus 133.55 for the learned boss. Equal win rates therefore do not establish better combat efficiency.

The takeaway is not that Q-tables outperform neural methods. It is that learning under an easier condition and generalizing to other opponents are different achievements. These profiles vary in several ways, so their comparison is not a single-variable causal experiment. Damage, hit opportunities, and action frequencies now provide diagnostics beyond win rate or loss.

For a portfolio, I want to show the C++/Python boundary, observed failures, evidence-backed iteration, and the decision to narrow scope. Codex assisted code generation and automated verification; I directed the goal, observed play, and reconsidered the approach. A fun adaptive boss, broad human-player generalization, superiority over the rule baseline, and measured inference cost remain unproven. The new environment code is locally committed as `211715b` but has not been pushed.

**Before changing models, in what order should state abstraction, rewards, and opponent difficulty be investigated when a boss cannot attack effectively or repeats one action?** My next step is controlled Q-table experiments, multiple training seeds, and unchanged evaluation opponents. Automatic curriculum promotion and mixed-opponent training are future work, not completed features.
