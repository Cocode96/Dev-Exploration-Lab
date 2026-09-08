# DX11 Update Break-even Experiment

단순한 이펙트 Update를 CPU 단일 스레드, 고정 작업 스레드, DX11 Compute Shader로 처리했을 때 작업 개수에 따른 손익분기점을 측정한다.

## 빌드

Visual Studio 2026에서 `Dx11UpdateBreakEven.sln`을 열고 `Release | x64`로 빌드한다. CMake는 사용하지 않는다.

```text
Client/Private/       실행 진입점과 실험 흐름
Benchmark/Public/     Benchmark 공개 인터페이스
Benchmark/Private/    CPU, 작업 스레드, DX11 측정 구현
Shader/               Compute Shader
```

명령줄에서는 Visual Studio Developer PowerShell에서 다음 명령을 사용한다.

```powershell
msbuild .\Dx11UpdateBreakEven.sln /m /p:Configuration=Release /p:Platform=x64
```

## 실행

```powershell
.\x64\Release\Dx11UpdateBreakEven.exe
```

실행 결과는 콘솔과 다음 파일에 기록된다.

- `reports/raw_results.csv`: 작업 개수별 7회 반복 원본
- `reports/summary_results.csv`: 7회 측정의 중앙값

## 비교 조건

- 모든 경로가 위치, 회전, 수명, World 행렬을 같은 방식으로 갱신한다.
- CPU 작업 스레드는 매 프레임 새 스레드를 만들지 않고 고정된 작업 스레드를 재사용한다.
- GPU 데이터는 StructuredBuffer에 유지하고 매 프레임 CPU로 Readback하지 않는다.
- GPU는 여러 프레임을 연속 Dispatch한 뒤 Timestamp Query로 실제 GPU 시간을 측정한다.
- Release x64 결과만 결론에 사용한다.

## Update 수식

각 데이터는 프레임 시간 `dt`에 대해 다음 순서로 갱신된다.

```text
v'     = v + g * dt
p'     = p + v' * dt
r'     = r + w * dt
life'  = life + dt
Mworld = S(scale) * Rz(r'.z) * T(p')
```

- `v`: 현재 속도
- `g`: 중력 가속도 `(0, -9.8, 0)`
- `p`: 현재 위치
- `r`: 현재 회전값
- `w`: 각속도
- `S`, `Rz`, `T`: 스케일, Z축 회전, 이동 행렬

CPU 단일, CPU 워커, GPU는 같은 수식을 실행하며, 계산을 배치하는 위치와 부가 비용만 다르다.

## 실행 방식별 비용 모델

Update 수를 `N`, Update 하나의 평균 계산 비용을 `C`, 실제 사용 워커 수를 `W`라고 두면 대략 다음처럼 표현할 수 있다.

```text
CPU 단일: T_single(N) = N * C

CPU 워커: T_worker(N) = T_wake + T_split + T_sync
                          + ceil(N / W) * C

GPU:       T_gpu(N) = T_submit + T_schedule
                       + T_kernel(N) + T_wait

개당 비용: C_per_update = T(N) / N
```

- `T_wake`: 대기 중인 워커를 깨우는 비용
- `T_split`: 작업 범위를 워커별로 분할하는 비용
- `T_sync`: 모든 워커가 끝날 때까지 동기화하는 비용
- `T_submit`: CPU가 Dispatch 명령을 제출하는 비용
- `T_schedule`: GPU가 Compute 작업을 스케줄링하는 비용
- `T_kernel`: GPU에서 실제 Update 수식을 실행하는 시간
- `T_wait`: Timestamp Query 결과와 GPU 완료를 기다리는 비용

CPU 워커의 손익분기점은 `T_worker(N) < T_single(N)`을 처음 만족하는 `N`이다. GPU는 실제 프레임 관점에서 `T_gpu(N) < T_single(N)`을 처음 만족하는 구간을 사용한다. GPU Kernel 시간만 비교하면 제출과 완료 대기 비용이 빠지므로 실제 손익분기점보다 유리하게 보일 수 있다.

## 측정값

- `cpu_single_us`: CPU 단일 스레드 프레임 시간
- `cpu_single_p95_us`: 7회 반복에서 얻은 프레임 평균값의 상위 95% 지연 시간
- `cpu_single_ns_per_update`: CPU 단일 스레드 중앙값을 Update 하나당 비용으로 환산한 값
- `cpu_workers_us`: 고정 작업 스레드 프레임 시간
- `cpu_workers_p95_us`: 7회 반복에서 얻은 프레임 평균값의 상위 95% 지연 시간
- `cpu_workers_ns_per_update`: 작업 스레드 중앙값을 Update 하나당 비용으로 환산한 값
- `gpu_submit_us`: CPU가 GPU Dispatch 명령을 제출하는 시간
- `gpu_kernel_us`: Timestamp Query로 측정한 GPU 실행 시간
- `gpu_wall_us`: Dispatch 제출부터 GPU 완료 확인까지의 전체 시간

작업 스레드와 GPU가 항상 빠르다는 결론을 전제하지 않는다. 작은 작업에서는 분배, 동기화, Dispatch 비용이 계산 시간보다 클 수 있다는 가설을 수치로 확인한다.

p95는 개별 프레임 840개의 분포가 아니라 120프레임 평균을 7회 반복한 값의 분포다. 현재 반복 수에서는 사실상 가장 느린 반복값에 해당하므로, 프레임 단위 tail latency가 필요하면 측정 구간을 별도로 확장해야 한다.

검증 결과와 해석은 [Q005 문서](../../../questions/game-development/q005-update-parallel-break-even/README.md)에 정리했다.
