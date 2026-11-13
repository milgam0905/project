# ARCANA Windows 빌드와 배포

새 PC에서 저장소 전체를 받은 뒤, Visual Studio 2022의 **C++를 사용한 데스크톱 개발**
워크로드(MSVC v143 및 Windows SDK)를 설치한다. `SimpleGame.sln`을 열어
`Release | x64`를 빌드한다. 32비트가 필요하면 솔루션의 `x86`(`Win32` 프로젝트)을 선택한다.
예를 들어 Developer PowerShell에서는:

```powershell
msbuild .\SimpleGame.sln /m /p:Configuration=Release /p:Platform=x64
```

GLFW 3.4와 GLEW 2.2.0의 공식 소스는 `third_party/`에 포함되어 있다. Visual Studio가
선택한 아키텍처로 해당 C 소스를 직접 컴파일하고 EXE에 정적으로 링크한다.
FreeGLUT, GLEW 및 GLFW DLL을 설치하거나 복사할 필요가 없다. MSVC 런타임도 정적으로 링크한다.
기존 저장소에 남아 있는 FreeGLUT/GLEW 바이너리는 새 빌드에 사용하지 않는다.
Windows 시스템 DLL과 OpenGL 2.1 호환 드라이버는 여전히 필요하다.
빌드 시 외부 패키지 다운로드나 Python 에셋 재생성은 필요하지 않다.

배포에는 새로 빌드된 `SimpleGame.exe`와 같은 출력 폴더의 `assets/` 및 `licenses/`를 함께 넣는다.
프로젝트는 DB, 로우폴리 스프라이트, 한글 대체 글꼴과 라이선스를 빌드 후 자동 복사한다.
이전 실행 폴더의 FreeGLUT/GLEW DLL은 새 배포에 포함하지 않는다.
이 절차는 Windows용이다. 창 라이브러리는 교체됐지만 게임은 GDI 글꼴, WIC 이미지,
Windows SQLite 및 WinHTTP를 사용하므로 Linux/macOS 빌드는 아직 지원하지 않는다.

전환 검증: Visual Studio 2022에서 Release x64/x86 및 Debug x64 컴파일·링크가 성공했다.
두 Release 실행 파일의 PE 의존성은 Windows 시스템 DLL뿐이고 필수 에셋 복사도 확인했다.
게임 실행·화면 비교·성능 측정은 수행하지 않았다.

## 동작 보존 및 확인 항목

- OpenGL 2.1 호환 컨텍스트, 기존 배치·셰이더·FBO 기능 검사·후처리 순서를 유지한다.
  framebuffer 픽셀 크기를 사용해 기존 1280×800 논리 화면 비율을 유지한다.
- 기존 16ms 업데이트 예약, 1/60초 고정 시뮬레이션, 한 번에 최대 8회 보정,
  시뮬레이션 부채 제한을 유지한다. 다음 예약까지 GLFW 이벤트 대기를 사용하므로
  프레임을 바쁜 루프로 반복하지 않는다. 새 VSync 설정은 강제하지 않는다.
- WASD, 숫자 키, Space/마우스 왼쪽 공격, Escape 및 기존 모드별 단축키를 유지한다.
  키 반복은 무시하고 창이 포커스를 잃으면 입력을 해제한다.
  Space와 마우스를 같이 누르면 둘 다 뗄 때 공격 입력이 해제된다.
- 한글 글꼴은 기존 맑은 고딕을 우선 사용한다. 선택된 글꼴에 한글 글리프가 없으면
  동봉한 Noto Sans CJK KR을 프로세스 전용으로 등록한다. 시스템에 글꼴을 설치하지 않는다.
  기존 글꼴 아틀라스, 글자 크기와 외곽선 경로를 그대로 사용한다.
  대체 글꼴 환경에서는 글자 모양이나 폭에 차이가 있을 수 있다.
- 사용자 실행 시 기본 모드 및 이전 모드의 창 크기 변경, 이동·공격, 포커스 이동 후
  입력 해제, 한글 제목/알림, 창 닫기와 저장·불러오기를 확인한다.
  같은 해상도·씬에서 기존 빌드와 `frame_interval_ms`, `game.update`, `render.frame_submit`,
  `render.swap_wait`, `draw_calls_per_frame`를 비교한다. 실행 성능과 화면 결과는 별도 확인 대상이다.

## 포함된 외부 소스 및 라이선스

- [GLFW 3.4](https://github.com/glfw/glfw/tree/3.4): 소스·헤더·빌드 보조 파일과 `LICENSE.md`.
- [GLEW 2.2.0](https://github.com/nigels-com/glew/releases/tag/glew-2.2.0): 배포 아카이브의 생성된 C 소스·헤더 및 `LICENSE.txt`.
- [Noto Sans CJK KR Regular 2.004](https://github.com/notofonts/noto-cjk/tree/Sans2.004/Sans/OTF/Korean):
  원본 OTF와 SIL Open Font License 1.1(`resource/fonts/OFL.txt`).
