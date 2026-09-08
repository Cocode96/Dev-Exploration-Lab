# WBOIT Lab project structure

이 문서는 현재 실험의 파일 목록보다 모듈 책임, 소유권과 교체 경계를 설명한다. 이후 Lab 프로젝트는 이 구조를 출발점으로 삼고, 더 빠르거나 안정적인 모듈이 확인되면 동일한 계약을 지키며 구현을 교체한다.

## Composition root and ownership

```text
wWinMain
  -> MainApp                                      unique ownership
    -> ApplicationContext                        unique ownership
      -> InputManager
      -> CameraManager
        -> FreeCamera
          -> TransformComponent                  value composition
          -> CameraComponent                     value composition
          -> FreeCameraControllerComponent       value composition
      -> SceneManager
        -> ValidationScene / EffectStressScene   unique ownership
          -> SkyBox / Terrain                    unique ownership
      -> BenchmarkManager
      -> Renderer
      -> DebugUiManager
    -> BenchmarkDebugPanel                       value composition
```

`ApplicationContext`는 엔진 모듈의 유일한 조립 지점이다. 각 Manager는 `std::unique_ptr`로 단독 소유하며, 사용 코드는 Context가 반환하는 참조로 접근한다. `BenchmarkDebugPanel`은 `MainApp`과 수명이 정확히 같고 다형성이 필요하지 않아 값 타입으로 둔다.

`Renderer`가 보관하는 `ApplicationContext*`와 ImGui가 초기화할 때 받는 `ID3D11Device*`, `ID3D11DeviceContext*`는 비소유 포인터다. 실제 수명은 `ApplicationContext`와 `Renderer`가 보장한다.

## Module responsibilities

| Module | Owns | Responsibility | Replaceable boundary |
|---|---|---|---|
| `MainApp` | Context, benchmark panel | Win32 생명주기, 프레임 순서, 전역 단축키 | 플랫폼 진입점 |
| `ApplicationContext` | Engine managers | 생성, 초기화 순서, 참조 제공 | 엔진 모듈 조립 |
| `InputManager` | Input state | Win32 입력 수집, sticky input 해제 | 입력 백엔드 |
| `CameraManager` | FreeCamera | 활성 카메라 갱신 | 카메라 정책 |
| `SceneManager` | Scenes | 장면 등록과 전환 | 장면 저장소 |
| `BenchmarkManager` | Samples, phases | 워밍업, 측정, CSV 출력 | 측정 시나리오 |
| `Renderer` | DX11 resources, scene texture | 장면 렌더링, GPU 타임스탬프, 장면 전용 캡처 | 그래픽스 백엔드 |
| `DebugUiManager` | ImGui context, log | UI 생명주기, 도킹, Scene View, 언어, 로그 | 런타임 디버그 UI 기반 |
| `BenchmarkDebugPanel` | UI-only state | 실험 조작, 방법 설명, 실시간 지표 | 프로젝트별 디버그 패널 |
| `EffectStressScene` | Procedural instances | 자연 볼륨과 강제 정렬 실패 데이터 생성 | 실험 데이터 생성기 |

## Runtime frame flow

```text
Win32 messages
  -> DebugUiManager input routing
  -> InputManager

MainApp::update
  -> apply previous Scene View size to Renderer targets
  -> update camera aspect ratio
  -> build ImGui frame
  -> Scene View records its available size for the next frame
  -> apply clicked benchmark settings
  -> BenchmarkManager::prepare_frame
  -> Scene::update
  -> CameraManager::update, only when UI and manual lock allow it

MainApp::render
  -> Renderer draws sky, terrain and transparency to the scene texture
  -> GPU timestamp range ends
  -> optional capture copies the scene texture only
  -> bind and clear the swap-chain back buffer
  -> Scene View samples the scene texture SRV
  -> DebugUiManager draws ImGui
  -> Present
  -> BenchmarkManager records the measured scene metrics
```

ImGui와 `Present`는 GPU 타임스탬프 범위 밖에 둔다. 런타임 도구 자체의 비용이 WBOIT 비교 결과에 섞이지 않도록 하기 위해서다. `Renderer`가 장면 텍스처를 소유하고 `DebugUiManager`는 해당 SRV를 비소유로 받아 표시한다. 도킹 레이아웃이 바뀌어도 장면 렌더 타깃과 카메라 종횡비가 다음 프레임에 함께 갱신된다.

## Reusable UI split

재사용 모듈과 실험별 모듈을 분리한다.

```text
Reusable engine module
  DebugUiManager
    Win32 + DX11 backend
    Debug-Hell theme
    Dockspace
    Scene View
    English / Korean state
    Runtime log
    Input capture protection

Project-specific module
  BenchmarkDebugPanel
    Scene selection
    Transparency method cards
    Stress controls
    CPU/GPU metrics
    Capture and benchmark commands
```

다음 Lab에서는 `DebugUiManager`를 그대로 사용하고 `BenchmarkDebugPanel`만 해당 실험용 패널로 교체한다.

## Module evolution rule

모듈을 교체할 때는 다음 순서를 따른다.

1. 기존 구현과 새 구현이 받는 입력과 내보내는 결과를 같은 조건으로 고정한다.
2. 성능, 안정성 또는 사용성 중 무엇을 개선하려는지 먼저 적는다.
3. 동일 장면, 동일 카메라, 동일 워밍업과 샘플 수로 비교한다.
4. 결과가 확인되기 전에는 기존 구현을 제거하지 않는다.
5. 빌드, 실행 화면, 수치와 로그를 확인한 뒤 기능 단위 커밋을 남긴다.

이 방식은 장기적인 생성형 게임 엔진 AI에서도 동일하다. AI가 명령을 생성하더라도 실제 변경 단위는 Renderer, Scene, Input, Debug UI처럼 계약이 분명한 엔진 모듈이어야 하며, 실행 결과와 로그를 통해 교체 전후를 검증한다.

## Current directory map

```text
dx11-wboit-benchmark/
  Client/
    Public/
      MainApp.h
      BenchmarkDebugPanel.h
      ValidationScene.h
      EffectStressScene.h
      WorldScene.h
      SkyBox.h
      Terrain.h
    Private/
      corresponding implementations
  Engine/
    Public/
      ApplicationContext.h
      DebugUiManager.h
      Renderer.h
      BenchmarkManager.h
      SceneManager.h
      InputManager.h
      Camera and component headers
    Private/
      corresponding implementations
  Shader/
    Transparency.hlsl
  ThirdParty/
    imgui/
      Dear ImGui core, Win32 backend, DX11 backend, license
  reports/
    reference/
      fixed benchmark evidence
    local/
      ignored local captures and runs
  Docs/
    PROJECT_STRUCTURE.md
  Dx11WboitBenchmark.sln
  WboitEngine.vcxproj
  WboitClient.vcxproj
```

## Current limits

- WBOIT는 근사 방식이며 정확한 투명도 합성이 아니다.
- 강제 정렬 실패 장면은 알고리즘 차이를 드러내기 위한 합성 최악 조건이다.
- 현재 수치는 독립 실험 결과이며 실제 게임 파이프라인의 성능을 대신하지 않는다.
- 실제 게임 주장으로 확장하려면 PIX 또는 RenderDoc 캡처와 실제 이펙트 데이터가 추가로 필요하다.

## 2026-09-08: 역할별 정의와 보조 함수 분리

- `Engine_Enum.h`, `Engine_Struct.h`: 공용 장면 enum과 렌더링 데이터.
- `Engine_Render_Struct.h`, `Engine_Render_Constant.h`, `Engine_Render_Function.h`: GPU 데이터 배치, 파이프라인 고정값, 셰이더 컴파일과 BMP 저장 함수.
- `Engine_Benchmark_Struct.h`, `Engine_Benchmark_Constant.h`, `Engine_Benchmark_Function.h`: 통계 데이터, 측정 프레임 수와 보고서 보조 함수.
- `Engine_DebugUi_Enum.h`, `Engine_DebugUi_Struct.h`, `Engine_DebugUi_Constant.h`, `Engine_DebugUi_Function.h`: 디버그 UI 정의와 보조 함수.
- `Engine_Function.h`, `Engine_Mesh_Function.h`: 투명도 모드 이름과 절차 생성 지형 보조 함수.
- `Client_Enum.h`, `Client_Constant.h`, `Client_Effect_Constant.h`, `Client_Function.h`: 스트레스 모드, 앱 설정, 공유 색상 팔레트와 패널 보조 함수.

헤더는 필요한 의존성을 직접 포함한다. 모든 정의를 한꺼번에 포함하는 통합 헤더와 PCH는 도입하지 않았다. 표준 라이브러리와 DirectX의 using 선언은 Engine 또는 Client 네임스페이스 안에 둔다. 클래스 전용 타입과 객체 소유권은 유지하고, 실행 중 계산되는 const 지역 변수는 사용 지점에 남긴다. 클래스 인덱스 변환용 constexpr 함수도 해당 클래스에 유지한다.

검증: Release x64 솔루션 빌드 통과. Engine/Client 공개 헤더 40개를 각각 단독 번역 단위로 MSVC C++20, /W4, /WX, /Zs로 검사해 모두 통과했다. git diff --check 통과. 실행 화면과 성능 수치는 이번에 재측정하지 않았다. 기존 README와 reference SUMMARY 3개의 미커밋 변경은 이번 수정에 포함하지 않았다.

## Visual Studio 필터 구성

Engine과 Client 모두 역할 아래 클래스별 하위 필터를 사용하며 같은 클래스의 .h와 .cpp를 함께 배치한다. 물리적인 Public/Private 디렉터리는 필터 분류에 사용하지 않는다. 공용 정의는 Enum, Struct, Constant, Function 역할별 하위 필터로 분류한다. Client 진입점은 EntryPoint, 셰이더와 문서는 기존 전용 필터에 둔다. 두 프로젝트의 파일 등록, 필터 참조, 부모 필터와 클래스별 헤더/구현 쌍을 검증했다.
