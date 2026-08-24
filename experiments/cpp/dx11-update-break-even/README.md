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

## 측정값

- `cpu_single_us`: CPU 단일 스레드 프레임 시간
- `cpu_workers_us`: 고정 작업 스레드 프레임 시간
- `gpu_submit_us`: CPU가 GPU Dispatch 명령을 제출하는 시간
- `gpu_kernel_us`: Timestamp Query로 측정한 GPU 실행 시간
- `gpu_wall_us`: Dispatch 제출부터 GPU 완료 확인까지의 전체 시간

작업 스레드와 GPU가 항상 빠르다는 결론을 전제하지 않는다. 작은 작업에서는 분배, 동기화, Dispatch 비용이 계산 시간보다 클 수 있다는 가설을 수치로 확인한다.

검증 결과와 해석은 [Q005 문서](../../../questions/game-development/q005-update-parallel-break-even/README.md)에 정리했다.
