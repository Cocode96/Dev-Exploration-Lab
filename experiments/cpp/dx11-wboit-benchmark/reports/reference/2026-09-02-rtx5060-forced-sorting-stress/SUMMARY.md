# Forced sorting stress reference

이 결과는 실제 게임 리소스를 사용하지 않은 독립적인 합성 벤치마크다. 동일 카메라에서 투명 오브젝트 제출 순서만 뒤집어 일반 알파 블렌딩과 WBOIT의 순서 민감도를 비교했다.

전체 실행 UI와 새 `Scene View` 구도는 [`runtime-ui.png`](runtime-ui.png)에서 확인할 수 있다.

## Environment

- GPU: NVIDIA GeForce RTX 5060 Laptop GPU
- Build: Release x64
- Window: 1280 x 720
- Scene View render target: 843 x 492
- Scene: Particle Stress, Forced sorting failure
- Workload: 1024 transparent instances
- Sampling: phase당 warm-up 60 frames, measured 300 frames, 총 24 phases
- GPU timing boundary: sky, terrain, transparency 포함, ImGui와 Present 제외

강제 실패 프리셋은 중심점 깊이가 거의 같은 교차 리본과 서로 겹치는 두 파티클 시트를 생성한다. 이 구성에서는 오브젝트 중심점 하나로 정렬해도 픽셀마다 필요한 합성 순서를 표현할 수 없다.

## 1024-instance performance

| Method | GPU mean | GPU p95 | Transparency | Resolve | CPU submit | CPU Z sort | Measured CPU sum | Draw calls |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Unsorted Alpha | 0.038287 ms | 0.045024 ms | 0.023693 ms | 0 ms | 0.002570 ms | 0 ms | 0.002570 ms | 3 |
| CPU Z-Sorted Alpha | 0.036400 ms | 0.037248 ms | 0.022557 ms | 0 ms | 0.002729 ms | 0.062005 ms | 0.064734 ms | 3 |
| Weighted Blended OIT | 0.084617 ms | 0.088320 ms | 0.065289 ms | 0.004684 ms | 0.004922 ms | 0 ms | 0.004922 ms | 4 |

이 장면에서 WBOIT의 평균 GPU 시간은 정렬하지 않은 알파 블렌딩의 약 2.21배였다. 대신 CPU Z 정렬 비용을 제거하며, 제출 순서를 바꿔도 화면이 거의 유지됐다. 따라서 이 결과는 WBOIT를 성능 최적화가 아니라 CPU 정렬과 순서 의존성을 줄이기 위한 품질 및 안정성 절충안으로 해석해야 한다.

전체 원본 수치는 [`benchmark.csv`](benchmark.csv)에 있다.

## Submission-order sensitivity

아래 이미지는 Renderer가 `Scene View`용 렌더 타깃에 그린 순수 장면을 직접 저장한 것이다. ImGui는 포함되지 않는다.

| Method | Forward order | Reverse order | RGB RMSE |
|---|---|---|---:|
| Unsorted Alpha | [`unsorted-forward.png`](unsorted-forward.png) | [`unsorted-reverse.png`](unsorted-reverse.png) | 0.05304528 |
| Weighted Blended OIT | [`wboit-forward.png`](wboit-forward.png) | [`wboit-reverse.png`](wboit-reverse.png) | 0.00044902 |

WBOIT의 forward/reverse RMSE는 일반 알파보다 약 99.15% 낮았다. 이 수치는 제출 순서 민감도 진단값이며, 참조 정답 이미지에 대한 정확도 수치는 아니다. WBOIT 자체도 근사 합성 방식이므로 실제 이펙트 색과 밝기를 별도로 검수해야 한다.

## Portfolio-safe claim

> 독립 DX11 벤치마크에서 1,024개 투명 인스턴스의 제출 순서를 반전했을 때, WBOIT는 일반 알파 블렌딩 대비 화면 차이 RMSE를 99.15% 줄였다. 같은 조건에서 평균 GPU 시간은 0.0383 ms에서 0.0846 ms로 증가했고, CPU 중심점 정렬 0.0620 ms를 제거했다.

이 주장은 현재 합성 장면에만 적용된다. 원 프로젝트의 효과라고 표현하려면 실제 프로젝트에서 PIX 또는 RenderDoc 캡처와 동일 조건 프로파일링을 추가해야 한다.
