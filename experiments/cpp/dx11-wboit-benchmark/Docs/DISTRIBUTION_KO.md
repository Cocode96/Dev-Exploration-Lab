# DX11 WBOIT Benchmark 배포판 안내

## 실행

1. 배포 폴더의 `WboitBenchmark.exe`를 실행한다.
2. 오른쪽 `Experiment Controls`에서 장면과 투명도 방식을 선택한다.
3. 중앙 `Scene View` 위에 마우스를 올린 상태에서 `WASD`, `Q`, `E`로 이동한다.
4. `Scene View`에서 마우스 오른쪽 버튼을 누른 채 움직이면 시점을 회전한다.

UI 오른쪽 위 버튼으로 영어와 한국어를 전환할 수 있다. `Camera Input` 버튼은 카메라 입력을 수동으로 잠그거나 다시 활성화한다.

## 권장 비교 순서

1. `Particle Stress`를 선택한다.
2. `Forced sorting failure`를 선택한다.
3. `Unsorted Alpha`, `CPU Z-Sorted Alpha`, `Weighted Blended OIT`를 차례로 선택한다.
4. `Reverse submission order` 또는 자동 반전을 사용해 제출 순서에 따른 화면 변화를 확인한다.
5. `Live Performance`에서 CPU 정렬, GPU 투명도와 WBOIT 합성 비용을 비교한다.

WBOIT는 성능을 항상 개선하는 방식이 아니다. CPU 오브젝트 정렬과 제출 순서 의존성을 줄이는 대신 추가 렌더 타깃과 합성 패스로 GPU 비용이 증가할 수 있다.

## Windows 보안 안내

현재 실행 파일에는 상용 코드 서명 인증서가 적용되지 않았다. 웹에서 내려받은 새 실행 파일은 정상 파일이어도 Microsoft Defender SmartScreen에서 인식되지 않은 앱으로 표시될 수 있다.

- 먼저 배포 출처와 `SHA256SUMS.txt`의 해시가 현재 파일과 일치하는지 확인한다.
- SmartScreen 경고는 출처와 해시를 직접 확인한 경우에만 `추가 정보`와 `실행`을 선택한다.
- 백신이 악성 코드명으로 탐지하거나 파일을 격리했다면 백신을 끄거나 폴더 전체를 예외 처리하지 않는다. 탐지 정보를 확인하고 해당 백신사의 오탐 신고 절차를 사용한다.
- 공개 배포판은 동일한 게시자 신원으로 Authenticode 코드 서명을 지속하고, 가능하면 Microsoft Store 배포를 사용한다.

Microsoft 공식 설명:

- [SmartScreen reputation for Windows app developers](https://learn.microsoft.com/windows/apps/package-and-deploy/smartscreen-reputation)
- [SignTool](https://learn.microsoft.com/windows/win32/seccrypto/signtool)

## 파일 구성

- `WboitBenchmark.exe`: Release x64 실행 파일
- `Shader_Transparency.hlsl`: 런타임 셰이더 소스
- `msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll`: 애플리케이션 로컬 VC++ 런타임
- `Evidence/`: 측정 CSV, 결과 설명, forward/reverse 비교 이미지
- `Licenses/`: Dear ImGui 라이선스
- `SHA256SUMS.txt`: 배포 파일 SHA-256 목록

모든 지형, 하늘, 교차 지오메트리와 파티클은 코드로 절차 생성한다. 상용 게임 리소스는 포함하지 않는다.
