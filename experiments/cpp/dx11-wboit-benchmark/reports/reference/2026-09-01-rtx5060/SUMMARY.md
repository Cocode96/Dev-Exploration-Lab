# DX11 WBOIT reference measurement, 2026-09-01

## Environment

- GPU: NVIDIA GeForce RTX 5060 Laptop GPU
- GPU driver: 32.0.15.9201
- CPU: Intel Core Ultra 9 275HX
- OS: Windows 11 Pro, build 26200
- Build: Release x64, MSVC v145
- Resolution: 1280x720
- Warmup: 60 frames per mode
- Samples: 300 frames per mode
- Scene: 64, 256, and 1024 generated translucent quads
- WBOIT intermediate render-target footprint: 8.79 MiB, RGBA16F plus R16F

No game texture, mesh, executable, or other copyrighted asset was used.

## Result

| Instances | Unsorted alpha GPU mean/p95 | Z-sorted alpha GPU mean/p95 | WBOIT GPU mean/p95 | WBOIT / alpha mean | CPU Z-sort mean |
|---:|---:|---:|---:|---:|---:|
| 64 | 0.020555 / 0.020608 ms | 0.020472 / 0.020576 ms | 0.045458 / 0.045760 ms | 2.21x | 0.000543 ms |
| 256 | 0.033440 / 0.033664 ms | 0.033542 / 0.033760 ms | 0.083184 / 0.084736 ms | 2.49x | 0.002140 ms |
| 1024 | 0.089022 / 0.089664 ms | 0.088885 / 0.089600 ms | 0.256974 / 0.275904 ms | 2.89x | 0.013785 ms |

- Conventional alpha forward versus reverse order RMSE: 0.05724967
- WBOIT forward versus reverse order RMSE: 0.00030777
- Relative order-sensitivity reduction: 99.46%
- WBOIT versus object-center Z-sort RMSE: 0.15763601, image distance only, not a correctness score
- Total draw calls: 2 for every path in this controlled renderer
- Transparency-related draws: 1 for alpha paths, 2 for WBOIT accumulation and resolve

## Conclusion for portfolio wording

This run supports the claim that WBOIT strongly reduces visual dependence on draw order in an intersecting translucent scene. It does not support a claim that WBOIT improves FPS or GPU frame stability. On this hardware and scene, WBOIT increased mean GPU time by 2.21x to 2.89x, while remaining below 0.28 ms at the 1024-instance p95.

A defensible statement is:

> In a standalone DX11 benchmark using 1024 intersecting translucent instances, WBOIT reduced forward/reverse draw-order RMSE by 99.46% compared with unsorted alpha blending. The visual stability came with a measured GPU cost increase from 0.089 ms to 0.257 ms mean, with a 0.276 ms p95 on an RTX 5060 Laptop GPU.

The original game pipeline still requires PIX or RenderDoc evidence before making claims about production-frame behavior.
