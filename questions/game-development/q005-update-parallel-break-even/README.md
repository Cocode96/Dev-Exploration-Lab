# Q005. 작은 Update를 GPU나 멀티스레드로 옮기면 언제부터 빨라질까?

- [원본 Discussion #14](https://github.com/Cocode96/Dev-Exploration-Lab/discussions/14)
- [실험 코드](../../../experiments/cpp/dx11-update-break-even/README.md)
- [반복별 원본 결과](../../../experiments/reports/raw_results.csv)
- [중앙값 결과](../../../experiments/reports/summary_results.csv)

## 질문

단순한 이펙트 Update를 CPU에서 처리하지 않고 DX11 Compute Shader나 멀티스레드로 옮기면 항상 빨라질까? 예전에 약 200개의 Bone 행렬 변환을 Compute Shader로 처리했을 때 오히려 문제가 생긴 경험과 같은 현상인지 수치로 확인한다.

## 처음 가설

- 작업량이 작으면 CPU 단일 스레드가 가장 빠를 것이다.
- 멀티스레드는 작업 분배와 동기화 비용을 넘을 만큼 계산량이 커져야 이득일 것이다.
- Compute Shader는 Dispatch, GPU 스케줄링, 완료 대기 비용 때문에 작은 작업에서 손해일 것이다.
- 충분히 큰 독립 작업에서는 병렬 처리의 장점이 고정 비용을 넘어설 것이다.

## 검증 방법

동일한 입자형 데이터에 위치, 회전, 수명, SRT World 행렬 갱신을 수행했다.

- CPU 단일 스레드
- 매 프레임 생성하지 않고 재사용하는 작업 스레드 풀
- DX11 StructuredBuffer와 Compute Shader

64개부터 262,144개까지 측정했다. 각 항목은 30프레임 워밍업 후 120프레임을 측정하고, 이 과정을 7회 반복한 중앙값이다. GPU Kernel 시간은 DX11 Timestamp Query로 측정했다. GPU 데이터는 GPU에 유지하고 CPU Readback은 하지 않았다.

## 측정 환경

- Windows x64
- Visual Studio 2026, MSVC v145, C++20, Release 최적화
- Direct3D 11
- GPU: Intel(R) Graphics
- 작업 스레드 풀: 23개, 작업량에 따라 실제 사용할 스레드 수를 조절

## 결과

단위는 μs다.

| Update 수 | CPU 단일 | 작업 스레드 | GPU Kernel | GPU 완료 대기 포함 |
|---:|---:|---:|---:|---:|
| 64 | 0.812 | 24.212 | 2.471 | 5.678 |
| 128 | 1.616 | 25.008 | 3.372 | 8.810 |
| 200 | 2.533 | 23.892 | 7.738 | 8.912 |
| 256 | 3.233 | 25.229 | 15.704 | 21.562 |
| 512 | 6.456 | 24.331 | 7.062 | 8.258 |
| 1,024 | 12.961 | 24.208 | 11.541 | 12.788 |
| 4,096 | 52.281 | 40.351 | 39.391 | 40.743 |
| 16,384 | 210.233 | 73.536 | 143.234 | 144.995 |
| 65,536 | 852.191 | 162.973 | 785.306 | 789.320 |
| 262,144 | 3,816.837 | 457.508 | 2,820.594 | 2,825.986 |

## 확인된 결론

약 200개의 작은 독립 Update에서는 CPU 단일 스레드가 가장 빨랐다. 작업 스레드는 계산보다 분배와 동기화 비용이 컸고, GPU도 실제 Kernel 실행과 완료 비용이 계산량보다 컸다. 따라서 단순히 Update를 GPU나 멀티스레드로 옮기는 것만으로는 최적화가 되지 않는다.

이 환경에서 작업 스레드는 4,096개 부근부터 단일 스레드와 경쟁하기 시작했고 16,384개부터 확실한 이득을 보였다. GPU는 65,536개 부근에서 단일 CPU와 비슷해졌으며, 262,144개에서 단일 CPU보다 빨랐다. Intel 내장 GPU에서는 큰 작업에서도 작업 스레드가 가장 빨랐다.

핵심은 CPU, 멀티스레드, GPU 중 하나를 미리 정하는 것이 아니라 실제 작업량과 데이터 이동, 동기화 지점을 포함해 측정하는 것이다.

## 게임 프로젝트에 적용할 판단 기준

- 수백 개 수준의 단순 이펙트 Update는 먼저 CPU 단일 스레드 구현과 프로파일링을 기준으로 삼는다.
- 멀티스레드는 작은 Job을 개별 제출하지 않고 충분히 큰 묶음으로 나눈다.
- GPU로 옮길 때는 결과가 계속 GPU에서 소비되는 경로를 우선한다.
- CPU가 같은 프레임에 결과를 읽어야 한다면 Readback과 동기화 비용을 별도로 측정한다.
- Bone처럼 부모와 자식의 계산 순서가 있는 작업은 독립 입자 Update와 분리해 검증한다.

## 한계

- 한 대의 PC와 Intel(R) Graphics에서 측정한 결과이므로 절대 손익분기점을 다른 환경에 그대로 적용할 수 없다.
- GPU 256개와 512개 구간에 비단조적인 변동이 관찰됐다. 드라이버와 GPU 스케줄링의 영향을 더 많은 환경에서 확인해야 한다.
- GPU에 유리하도록 Readback을 제외했다. 매 프레임 CPU Readback이 필요하면 GPU 경로는 더 불리해질 수 있다.
- Bone 계층의 부모 자식 의존성은 구현하지 않았다. 과거 Bone 사례와 같은 비용 구조의 방향은 확인했지만, Bone 전용 결론은 별도 실험이 필요하다.

## 공식 참고 자료

- [ID3D11DeviceContext와 Dispatch](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nn-d3d11-id3d11devicecontext)
- [ID3D11DeviceContext::GetData](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-getdata)
- [D3D11_QUERY_DATA_TIMESTAMP_DISJOINT](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_query_data_timestamp_disjoint)
- [Direct3D 11 Compute Shader](https://learn.microsoft.com/en-us/windows/win32/direct3d11/direct3d-11-advanced-stages-compute-shader)
