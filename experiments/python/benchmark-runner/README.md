# Experiment Runner

명령줄 프로그램의 여러 구현을 같은 조건으로 반복 실행하고 CSV와 Markdown 보고서를 생성하는 작은 도구다. Python 표준 라이브러리만 사용한다.

## EXE를 직접 선택해 실행하기

설정 파일 없이 실행하면 필요한 값을 순서대로 물어본다.

```powershell
python run_experiment.py
```

비교 대상은 서로 다른 EXE 두 개여도 되고, 같은 EXE에 서로 다른 인자를 사용해도 된다.

```text
Experiment name [exe-comparison]: DX-renderer-switch
First variant name [variant_a]: single_dll
single_dll EXE path: C:\MyExperiment\renderer_test.exe
single_dll arguments (optional): --mode single-dll
Second variant name [variant_b]: swap_dll
swap_dll EXE path: C:\MyExperiment\renderer_test.exe
swap_dll arguments (optional): --mode swap-dll
```

입력한 조건은 결과 폴더의 `experiment_config.json`에 함께 저장된다. 다음에는 해당 파일을 지정하여 동일한 조건으로 다시 실행할 수 있다.

```powershell
python run_experiment.py results\DX-renderer-switch-실행시각\experiment_config.json
```

저장된 설정에는 원래 작업 폴더도 기록되므로, EXE가 같은 폴더의 DLL이나 데이터 파일을 상대 경로로 읽는 경우에도 실행 조건이 유지된다.

## 포함된 예제로 빠르게 확인하기

이 폴더에서 다음 명령을 실행한다.

```powershell
python run_experiment.py sample_experiment.json
```

결과는 `results/<실험 이름>-<실행 시각>/` 아래에 생성된다.

- `raw_results.csv`: 모든 개별 실행 결과
- `report.md`: 구현별 성공 횟수, 중앙값, P95, 최솟값, 최댓값
- `experiment_config.json`: 같은 조건으로 다시 실행할 수 있는 설정

여러 구현의 성능을 공개 문서에서 비교할 때는 `raw_results.csv`를 측정 원본으로 유지하고, 기준 구현 대비 Speedup과 Normalized Time 같은 파생 지표를 별도 요약 CSV에 기록한다. 파생 지표는 기존 시간을 나눈 값이므로 재측정 없이 계산할 수 있으며, 어떤 구현을 `1.0` 기준으로 사용했는지 열 이름과 문서에 명시한다.

## 설정 파일

```json
{
  "name": "DX11-DX12-renderer-switch",
  "iterations": 100,
  "warmup": 10,
  "timeout_seconds": 30,
  "random_seed": 20260819,
  "output_directory": "results",
  "variants": [
    {
      "name": "single_dll",
      "command": ["renderer_test.exe", "--mode", "single-dll"]
    },
    {
      "name": "swap_dll",
      "command": ["renderer_test.exe", "--mode", "swap-dll"]
    }
  ]
}
```

`command`는 셸 명령 문자열이 아니라 인자 단위의 문자열 배열이다. 따라서 공백이 포함된 경로도 별도의 이스케이프 처리 없이 한 항목에 넣으면 된다.

`{python}`을 실행 파일 자리에 사용하면 Experiment Runner를 실행한 것과 같은 Python을 사용한다. 설정 파일 안의 상대 경로는 해당 설정 파일이 있는 폴더를 기준으로 실행된다.

측정 순서는 항상 A 다음 B가 되지 않도록 `random_seed`를 기준으로 섞는다. 같은 seed를 사용하면 같은 순서를 다시 만들 수 있다.

## C++ 프로그램에서 내부 수치 전달하기

실행기에서 측정하는 `process_ms`에는 프로세스 시작과 종료 비용도 포함된다. 렌더러 전환 자체의 시간처럼 프로그램 내부 구간을 측정하려면 표준 출력에 다음 형식의 한 줄을 추가한다.

```cpp
std::cout
    << "EXPERIMENT_RESULT: {\"switch_ms\":" << switchMs
    << ",\"frame_spike_ms\":" << frameSpikeMs
    << ",\"debug_errors\":" << debugErrorCount
    << "}\n";
```

JSON 객체의 숫자 항목은 CSV 열과 Markdown 통계 항목으로 자동 추가된다. 이 줄 외의 출력은 자유롭게 사용해도 된다. 동일한 접두사로 여러 줄을 출력하면 마지막 줄만 사용한다.

## 코드 읽는 순서

`run_experiment.py`는 다음 순서로 읽으면 된다.

1. `load_config`, JSON 설정을 Python 객체로 변환한다.
2. `run_once`, 프로그램을 한 번 실행하고 결과 한 행을 만든다.
3. `make_run_order`, A/B 실행 순서를 섞는다.
4. `write_csv`, 원본 측정값을 저장한다.
5. `build_summary`, 중앙값과 P95 보고서를 만든다.
6. `run_experiment`, 위 작업을 전체 순서대로 호출한다.

## 측정 시 주의점

- 성능 비교는 Release 빌드로 수행한다.
- VSync, 해상도, GPU, 드라이버, 전원 설정을 동일하게 유지한다.
- Debug Layer를 켠 안정성 실험과 끈 성능 실험을 분리한다.
- 평균 하나보다 중앙값, P95, 실패 횟수를 함께 확인한다.
- 한 컴퓨터의 결과를 모든 컴퓨터에 적용되는 절대적인 결론으로 표현하지 않는다.
