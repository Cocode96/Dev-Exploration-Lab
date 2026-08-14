# Q004, Runtime Renderer DLL Swap

Discussion: [#4](https://github.com/Cocode96/Daily-Dev-Questions/discussions/4)

## 실제 DirectX Device 교체 실험

솔루션의 기본 시작 프로젝트는 `00_START_TextureRendererExperiment`입니다.

```text
00_START_TextureRendererExperiment.exe
├─ 통합 구조: RealDxCombinedRenderer.dll
│  ├─ DX11 Device, Swap Chain, Texture
│  └─ DX12 Device, Command Queue, Texture, Fence
└─ 분리 구조
   ├─ RealDx11Renderer.dll
   └─ RealDx12Renderer.dll
```

- 실행 시 DX12를 먼저 검사하고 실패하면 DX11로 fallback합니다.
- `Separate DX DLLs` 토글로 통합 DLL과 백엔드별 분리 DLL 구조를 선택합니다.
- `DX12 DLL / backend` 토글로 DX11과 DX12를 교체합니다.
- `Esc`를 누르면 프로그램을 종료합니다.
- `Texture objects` 슬라이더에서 GPU 텍스처 리소스를 가진 사각형 객체를 1개부터 1,000개까지 재생성합니다.
- DX11은 파란 체크 텍스처, DX12는 주황 체크 텍스처를 사용합니다.
- 텍스처 원본은 실행 폴더의 `Texture/`에서 Loader가 읽습니다.
- 창 제목과 ImGui에서 현재 Renderer를 확인할 수 있습니다.

```text
DirectX 11 → 파란 화면
DirectX 12 → 빨간 화면
```

교체할 때 기존 Renderer 객체를 먼저 파괴하고 새 Device와 Swap Chain을 생성합니다. DX12는 Fence로 GPU 완료를 기다립니다. CPU 원본 PPM을 유지하는 대신 전환 후 Loader가 다시 읽고, 선택한 개수만큼 GPU Texture와 SRV를 다시 만듭니다. `InputAssemblerComponent`는 텍스처 핸들을 가진 재사용 가능한 사각형 표시 컴포넌트이며 ImGui draw list의 vertex/index 입력으로 배치 렌더링됩니다.

## 이번 실험에서 확인하는 것

`1,000개면 유의미한가`에 대한 답은 조건부로 그렇습니다. 4x4 텍스처 1,000개는 VRAM 대역폭보다 API 객체 생성, descriptor 생성, upload, 동기화 비용을 확대합니다. 실제 대형 게임 텍스처 스트리밍 성능을 대표하지는 않습니다.

UI가 표시하는 값은 다음처럼 해석합니다.

| 값 | 포함 범위 |
|---|---|
| Last switch, seconds | 기존 Renderer 파괴와 GPU idle, 필요한 DLL load/unload, 새 Device/Swap Chain, ImGui backend, Texture 전체 재생성 |
| Switch avg/min/max | 실행 이후 백엔드 교체 표본의 누적 통계 |
| Texture resource build, seconds | 현재 백엔드에서 지정한 개수의 Texture와 SRV를 만드는 구간 |
| Frame CPU, seconds | UI 구성, 모든 격자 draw command 기록, Present와 현재 구현의 GPU 대기까지 포함한 CPU 경과 시간 |
| FPS | ImGui가 계산한 최근 프레임 속도 |

비교할 때는 Release 빌드를 사용하고 1, 10, 100, 500, 1,000개를 각각 여러 번 왕복합니다. 최초 표본은 DLL/드라이버/파일 캐시 warm-up 영향을 받으므로 버리고, 최소값보다 중앙값을 별도로 기록하는 것이 좋습니다. 현재 UI의 평균은 빠른 관찰용 누적 평균입니다.

`Texture Objects` 창은 객체 수의 제곱근을 기준으로 행과 열을 자동 계산하고, 현재 창 안에 모든 사각형을 겹치지 않게 압축 배치합니다. 따라서 1,000개를 선택하면 1,000개의 서로 다른 SRV를 사용하는 textured quad가 매 프레임 draw data에 실제로 제출됩니다. 화면 밖 항목을 생략하는 스크롤 목록 방식은 사용하지 않습니다.

통합 구조에서 DX11/DX12를 바꾸면 `RealDxCombinedRenderer.dll`은 메모리에 유지되고 Renderer 객체만 교체됩니다. 분리 구조에서는 백엔드를 바꿀 때 현재 DLL을 `FreeLibrary`한 뒤 다른 백엔드 DLL을 `LoadLibrary`합니다. 따라서 두 구조의 `Last switch` 차이에 실제 모듈 교체 비용이 반영됩니다. DLL 구조만 바꾸는 경우에도 동일한 Texture 개수로 Renderer를 다시 만들어 비교합니다.

`DLL Switch Comparison` 창은 통합 DLL 내부 전환과 분리 DLL 교체의 표본을 서로 섞지 않고 별도로 누적합니다. 각 표본은 DLL module 단계와 Renderer/Texture 재생성 단계의 시간으로 나뉘며, 두 구조의 평균 차이도 표시합니다. 정확한 비교를 위해 같은 Texture 개수와 같은 DX11/DX12 전환 방향에서 반복 측정해야 합니다.

## ImGui 스레드 결정

ImGui와 Present는 Renderer와 같은 메인 스레드에서 실행합니다. Win32 메시지, ImGui context, DX11 immediate context, DX12 command list와 Swap Chain을 별도 UI 스레드가 동시에 소유하면 안전하지 않습니다. 도킹 UI 때문에 느려지는 정도 자체는 `Frame CPU`와 FPS로 관찰할 수 있습니다. 이후 CPU 작업을 분리하려면 파일 디코딩이나 실험 데이터 집계를 worker thread로 보내고, GPU resource 생성과 ImGui draw 제출은 render thread에 남기는 방식이 적절합니다.

## 검증 범위

- `LoadLibraryW`와 `GetProcAddress`를 이용한 런타임 백엔드 선택
- C ABI 팩토리 함수 조회
- 객체를 생성한 DLL의 `DestroyRenderer`로 객체 파괴
- 렌더러 객체 파괴 후 `FreeLibrary` 수행
- DLL 교체 이후 새 백엔드 생성

## 빌드

Visual Studio 2022와 CMake 기준입니다.

```powershell
cmake -S . -B build
cmake --build build --config Release --target 00_START_TextureRendererExperiment
```

CMake는 빌드 시스템 그 자체라기보다 빌드 파일 생성기입니다. 위 명령은 현재 환경에서 Visual Studio 솔루션과 프로젝트를 `build/`에 생성하고, 두 번째 명령이 MSBuild로 컴파일합니다.

## 실행

```powershell
.\build\Release\real_dx_switch.exe
```

환경에 따라 실행 파일과 DLL이 `build\Release` 대신 다른 구성 폴더에 생성될 수 있습니다.

## 이 기능을 만드는 실제 이유, 실행 환경 호환성

단순히 실행 중 교체해 보고 싶다는 실험을 넘어, 여러 Renderer 백엔드를 제공하는 실용적인 이유는 사용자 PC마다 지원하는 그래픽 환경이 다를 수 있기 때문입니다.

정확히는 DLL 파일 자체의 호환성만 확인하는 것이 아닙니다. 각 백엔드가 요구하는 조건을 검사합니다.

- 운영체제가 해당 Direct3D 런타임을 제공하는가
- 선택한 GPU Adapter와 드라이버가 Device 생성을 지원하는가
- 엔진이 요구하는 최소 Feature Level을 만족하는가
- Shader Model과 선택 기능의 Tier를 만족하는가
- Device와 Swap Chain 초기화가 실제로 성공하는가

Direct3D API 버전과 Feature Level은 같은 개념이 아닙니다. 예를 들어 D3D12 백엔드도 최소 `D3D_FEATURE_LEVEL_11_0`을 요구해 Device 생성을 시도할 수 있고, 더 높은 기능이 필요하면 `CheckFeatureSupport`로 별도 확인해야 합니다.

### 백엔드 탐지 인터페이스

```cpp
struct RendererSupport
{
    bool supported;
    int featureLevel;
    const char* reason;
};

RendererSupport ProbeRenderer(
    GraphicsBackend backend,
    AdapterHandle adapter);
```

DX12는 실제 Device를 보관하지 않고 지원 가능성만 시험할 수도 있습니다.

```cpp
const HRESULT result = D3D12CreateDevice(
    adapter,
    D3D_FEATURE_LEVEL_11_0,
    __uuidof(ID3D12Device),
    nullptr);

const bool supportsDx12 = SUCCEEDED(result);
```

### 시작 시 자동 선택

```text
1. DXGI로 Adapter 열거
2. 소프트웨어 Adapter 제외 또는 WARP 사용 여부 결정
3. DX12 백엔드 지원 검사
4. DX12 Device와 필수 기능 초기화 시도
5. 실패하면 DX11 지원 검사와 초기화
6. 모두 실패하면 오류 원인과 최소 사양 안내
```

사용자 옵션과 자동 선택을 함께 제공할 수 있습니다.

```text
--renderer=auto    지원되는 우선순위에 따라 선택
--renderer=dx12    DX12를 요청하고 실패 이유 출력
--renderer=dx11    호환성 또는 디버깅을 위해 강제 선택
```

### 분리 DLL이 주는 추가 이점

호환되지 않는 백엔드 DLL을 로드하지 않고 건너뛸 수 있습니다. 백엔드가 선택적인 SDK 또는 런타임에 의존한다면 프로세스 시작 자체가 실패하는 위험도 분리할 수 있습니다.

다만 하나의 통합 DLL에서도 Direct3D 런타임을 명시적으로 동적 로드하고 백엔드 초기화를 지연하면 같은 선택 로직을 구현할 수 있습니다. 따라서 환경 호환성이 곧바로 백엔드별 DLL 분리를 강제하는 것은 아닙니다.

### 현재 설계 결정

```text
목적
→ 여러 사용자 환경에서 지원 가능한 Renderer를 탐지하고 선택한다.

우선 설계
→ 하나의 Renderer DLL 안에서 DX12를 먼저 검사하고 DX11로 fallback한다.

분리 조건
→ 독립 배포, 선택 의존성, 플러그인 확장 또는 장애 격리가 실제로 필요해질 때 백엔드별 DLL로 나눈다.
```

즉, Renderer 교체 기능은 단순한 기술 시연이 아니라 호환성 확보, 백엔드 비교, 문제 발생 시 fallback을 제공하는 기능으로 발전시킬 수 있습니다.

## 이 실험이 증명하지 않는 것

- DX9, DX11, DX12 GPU 리소스 호환성
- GPU 작업 완료 대기와 Swap Chain 재생성
- 렌더 스레드, 콜백, 함수 포인터의 실제 종료 동기화

현재 구현은 CPU 원본을 Loader로 다시 읽고 GPU idle을 기다리지만, 실제 그래픽 API 교체에서는 비동기 스트리밍, 큰 mip chain, descriptor allocator, resize 처리, device-lost 복구와 DLL 함수 포인터의 수명 검증이 추가로 필요합니다.
