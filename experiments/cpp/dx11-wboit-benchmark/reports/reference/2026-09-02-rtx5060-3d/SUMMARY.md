# DX11 WBOIT 3D reference measurement, 2026-09-02

## Environment

- GPU: NVIDIA GeForce RTX 5060 Laptop GPU
- GPU driver: 32.0.15.9201
- CPU: Intel Core Ultra 9 275HX
- Build: Release x64, MSVC v145
- Resolution: 1280x720
- Warmup: 60 frames per phase
- Samples: 300 frames per phase
- Scene: procedural sky, generated terrain, depth buffer, world-space effects
- Camera: fixed at the default startup transform during measurement

No game texture, mesh, executable, or other copyrighted asset was used.

## Result

| Instances | Unsorted alpha mean/p95 | Z-sorted alpha mean/p95 | WBOIT mean/p95 | WBOIT / alpha mean | CPU Z-sort mean |
|---:|---:|---:|---:|---:|---:|
| 64 | 0.024618 / 0.024736 ms | 0.024654 / 0.024832 ms | 0.042393 / 0.042592 ms | 1.72x | 0.000739 ms |
| 256 | 0.032410 / 0.032576 ms | 0.032202 / 0.032352 ms | 0.064948 / 0.065184 ms | 2.00x | 0.003504 ms |
| 1024 | 0.064373 / 0.064672 ms | 0.067032 / 0.068832 ms | 0.156877 / 0.158208 ms | 2.44x | 0.026765 ms |

- Total draws: 3 for alpha paths, 4 for WBOIT
- Transparency draws: 1 for alpha paths, 2 for WBOIT accumulation and resolve
- WBOIT resolve mean at 1024 instances: 0.008947 ms
- WBOIT transparency mean at 1024 instances: 0.123169 ms

## Interpretation

The current 3D run measures the complete synthetic render workload before presentation, including sky, terrain, and transparency. At 1024 instances, WBOIT increased mean GPU time from 0.064373 ms to 0.156877 ms, or about 2.44x. CPU object-center sorting cost 0.026765 ms mean at the same count.

This run does not include fresh forward-versus-reverse image RMSE. The earlier 99.46 percent order-sensitivity reduction belongs only to the screen-space reference run and must not be quoted as a result of this 3D scene. A new paired image capture and metric pass is the next validation task.
