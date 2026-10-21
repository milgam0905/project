# 이전 범용 프로토타입 — 원본 DB 통합에는 사용하지 마세요

아래 내용은 원본 DB를 받기 전 작성한 설계 기록입니다. 현재 통합 절차는 루트 README.ko.md와 bp::native를 사용합니다.

# Blood Principle: 기존 OpenGL/C++ 게임용 독립 통합 모듈

이 패키지는 C++17 정적 라이브러리와 연결 예제입니다. 기존 프로젝트의 디렉터리, 실행 파일, OpenGL 렌더러, 입력·창·카메라, 전투 시스템을 유지하고 필요한 소스만 추가합니다. 별도 에디터나 개발 도구로 전환할 필요가 없습니다.

## 확인된 범위와 미확인 자료

참조 대화에서 확인한 요구: NPC의 거래·살인·약탈·관계 형성, 도적 활동에 따른 기사/마법사 파견, 마수의 거주지 유지와 굶주림·유인에 의한 침공, 사냥에 따른 마력 증가 및 약한 마수 반복 사냥의 효율 감소.

현재 작업 공간에는 `blood_principle_game.db`, 기존 게임 소스, 39개 WBG의 정의가 없습니다. 참조 대화의 상세 답변도 내용 대신 참조 표시만 반환되었습니다. 따라서 실제 DB와의 호환성 및 원래 39개 규칙의 실행 의미는 아직 검증하지 않았습니다. 이 패키지는 원본 DB나 WBG를 재작성하지 않습니다. `mapping/*.sql`은 명시적인 예시 계약이며 실제 스키마에 맞춰 수정해야 합니다. 테스트에서 사용하는 39개 행은 실행 경로 검증 전용 가상 데이터입니다.

WBG 약어의 뜻·분류·이름·효과를 추정하지 않았습니다. 원래 ID/name/payload를 읽고, 39개 전체에 대응하는 게임별 처리 함수를 등록하는 확장 지점을 제공합니다. 원본별 처리 함수는 정의 자료를 받은 뒤 구현해야 하며 현재 제공되는 기본 시뮬레이션이 곧 원래 39개 WBG 전체의 구현이라는 의미는 아닙니다.

## 구성

| 경로 | 용도 |
|---|---|
| `include/bp/simulation.hpp`, `src/simulation.cpp` | OpenGL·SQLite에 의존하지 않는 자율 시뮬레이션 |
| `include/bp/content.hpp`, `src/content.cpp` | SQLite 읽기 전용 로더·검증 |
| `mapping/*.sql` | 원래 DB와 모듈 사이의 SELECT 매핑 |
| `examples/host_integration.cpp.txt` | 기존 초기화·업데이트 코드에 넣는 연결 예제; 일부 게임별 함수는 구현 필요 |
| `examples/main.cpp` | DB 없이 실행되는 콘솔 예제 |
| `tools/inspect_db.py` | 원본 DB의 테이블·뷰 정의를 읽기 전용으로 출력 |
| `tests/` | 시뮬레이션 및 SQLite 계약 검증 |

## 1. 원본 스키마 연결

원본 DB는 기존 assets 경로에 그대로 둡니다. 모듈은 `SQLITE_OPEN_READONLY`로 열고 하나의 읽기 트랜잭션으로 콘텐츠를 가져옵니다. INSERT/ALTER/마이그레이션은 실행하지 않습니다. 시작할 때만 DB를 읽고 매 프레임 SQL을 실행하지 않습니다.

```powershell
python tools/inspect_db.py "D:/YourGame/assets/blood_principle_game.db" > schema.json
```

SQL 매핑은 각각 하나의 SELECT이며 아래 순서의 열을 반환해야 합니다. 이름보다 순서가 계약입니다. NULL은 허용하지 않으므로 선택 필드에는 필요하면 `COALESCE()`를 적용합니다. 기본값은 설정의 의미를 확인한 뒤 지정하세요.

| 쿼리 | 반환 열 순서 |
|---|---|
| regions | id(INTEGER), threat(숫자, 0 이상) |
| actors | id(INTEGER), archetype(INTEGER), kind(TEXT), role(TEXT), region(INTEGER), home(INTEGER), power(숫자, 양수), mana(숫자), hunger(숫자 0~1), gold(숫자), food(숫자), alive(INTEGER 0/1) |
| wbg | id(TEXT), name(TEXT), payload(TEXT) |

`kind`는 `npc` 또는 `beast`; `role`은 `civilian`, `bandit`, `knight`, `mage`입니다. 마수의 role은 `civilian`으로 매핑해도 마수 행동을 따릅니다. 마력·재화·식량은 0 이상입니다. 지역 ID와 개체 ID는 양의 정수이며 모든 NPC/마수 개체의 ID는 전역에서 유일해야 합니다. 원본에 텍스트 ID 또는 테이블별 중복 ID가 있으면 게임 측에 안정적인 정수 ID 대응표를 두세요. 매 실행마다 바뀌는 임시 행 번호는 저장 데이터에 쓰지 마세요.

NPC/마수가 별도 테이블이면 `UNION ALL`로 묶습니다. 종류 데이터만 있고 개체 데이터가 없다면 게임의 기존 스폰 시스템에서 `Actor`를 만들고 `Snapshot`에 추가합니다. 원본 마수 종류 ID는 `archetype`으로 연결하여 종류별 반복 사냥 기록을 유지합니다. `power`는 서로 비교 가능한 전투력 지표이며 실제 공식은 게임에서 정합니다. 이름·좌표·장비·대사 등 기존 게임 필드는 게임에 계속 보관합니다.

WBG가 여러 열에 나뉘어 있으면 SQL에서 payload 문자열을 조립하거나 로더를 해당 계약으로 확장합니다. JSON 생성 함수를 쓰려면 사용 중인 SQLite 빌드의 지원을 확인하세요. 로더 자체는 JSON 해석을 하지 않습니다. 등록 시 39개 수량, 중복/빈 ID, 누락 처리 함수를 검사합니다. 수량만 맞아도 의미상 39개 원본과 일치하는지는 보장하지 않으므로 실제 ID 목록과 정의를 대조해야 합니다.

## 2. 기존 프로젝트에 추가

### 기존 CMake 프로젝트

원하는 하위 디렉터리에 이 폴더를 복사한 뒤 기존 CMakeLists에 추가합니다. `YourGame`은 기존 타깃 이름으로 바꿉니다.

```cmake
add_subdirectory(external/blood_principle_module)
target_link_libraries(YourGame PRIVATE bp::sim bp::content)
```

SQLite 헤더와 라이브러리가 설치되어 있으면 `find_package(SQLite3)`로 찾습니다. 기존 프로젝트의 SQLite와 같은 버전·아키텍처·런타임을 사용하세요. 이 모듈은 새 CMake의 `SQLite3::SQLite3`와 이전 `SQLite::SQLite3` 타깃을 모두 처리합니다.

이미 프로젝트가 SQLite를 직접 관리한다면 `src/content.cpp`와 include 경로를 기존 게임 타깃에 직접 추가하고 기존 SQLite에 링크하는 방법이 간단합니다. SQLite 구현을 중복 링크하지 마세요.

### Visual Studio .vcxproj 등 CMake를 쓰지 않는 프로젝트

1. 기존 프로젝트에 `src/simulation.cpp`, `src/content.cpp`를 소스 파일로 추가합니다.
2. 이 모듈의 `include`와 SQLite 헤더 폴더를 추가 포함 디렉터리에 등록합니다.
3. C++ 언어 표준을 C++17 이상으로 설정합니다. Windows에서는 `/utf-8` 사용을 권장합니다.
4. 기존 `sqlite3.lib`에 연결합니다. DLL 빌드라면 해당 `sqlite3.dll`을 실행 파일 옆에 배치합니다. x64/x86 및 Debug/Release 런타임을 맞춥니다.
5. 또는 공식 amalgamation의 `sqlite3.c`를 **C 소스**로 한 번만 컴파일합니다. SQLite 자체를 C++로 강제 컴파일하지 마세요.
6. 기존 OpenGL 관련 링크 설정은 유지합니다. 이 모듈에는 GL/GLFW/SDL 의존성이 없습니다.

### 모듈만 별도 빌드·검증

Developer PowerShell for Visual Studio에서 모듈 폴더 기준:

```powershell
cmake -S . -B build -A x64 -DBP_BUILD_TESTS=ON -DBP_BUILD_EXAMPLE=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
./build/Release/bp_example.exe
```

SQLite를 찾지 못하면 설치된 라이브러리 위치를 지정합니다.

```powershell
cmake -S . -B build -A x64 -DSQLite3_INCLUDE_DIR="D:/deps/sqlite/include" -DSQLite3_LIBRARY="D:/deps/sqlite/lib/sqlite3.lib" -DBP_BUILD_TESTS=ON
```

공식 amalgamation 소스를 직접 빌드하는 대안도 있습니다. 소스는 패키지에 포함하지 않았습니다.

```powershell
cmake -S . -B build -A x64 -DBP_SQLITE_SOURCE_DIR="D:/deps/sqlite-amalgamation" -DBP_BUILD_TESTS=ON -DBP_BUILD_EXAMPLE=ON
```

SQLite 없이 시뮬레이션만 먼저 검증할 수 있습니다.

```powershell
cmake -S . -B build-sim -A x64 -DBP_WITH_SQLITE=OFF -DBP_BUILD_TESTS=ON -DBP_BUILD_EXAMPLE=ON
cmake --build build-sim --config Release
ctest --test-dir build-sim -C Release --output-on-failure
```

Linux/macOS에서는 `-A x64`를 빼고 생성기에 맞는 실행 경로를 사용합니다. 이 패키지의 현장 검증은 Windows/MSVC 기준이며 다른 플랫폼 실행은 별도 확인이 필요합니다.

## 3. 기존 게임 루프 연결

`examples/host_integration.cpp.txt`를 참고하여 시작할 때 `loadContent()` → `World::restore()` → `setWbg()`를 호출합니다. 예외 발생 시 기존 게임의 오류 화면/로그로 전달하고 초기화를 중단하세요. 미완성 WBG를 빈 함수로 채워 통과시키지 마세요.

기존 `update(deltaSeconds)`에서 `advance()`를 호출하고 `drainEvents()`를 읽습니다. 이벤트의 ID를 기존 엔티티에 연결하여 UI, 대화, 퀘스트, 목적지 지시 등을 갱신합니다. 거래·약탈·사망·마력 수치는 World에 이미 반영되므로 이벤트를 보고 다시 계산하지 않습니다. 해당 필드의 권위는 World 한 곳에 두고 게임 엔티티에 복사하거나, 기존 시스템과의 명령 동기화 계층을 명시적으로 구현하세요. 양쪽에서 동시에 변경하면 안 됩니다.

기존 전투가 처치를 확정하면 `kill(killerId, victimId)`를 호출합니다. 다른 지역·죽은 개체·자기 자신 처치는 거부하며 같은 처치를 다시 보고해도 마력을 재지급하지 않습니다. 렌더링은 기존 데이터를 그대로 사용합니다. region은 거시적 소속이며 화면의 x/y/z 이동·경로 탐색은 기존 시스템에서 수행합니다.

## 4. 기본 자율 행동과 조정 지점

아래 수치·공식은 이번 모듈의 **조정 가능한 기본 설계**이며 이전 대화의 확정 수치가 아닙니다.

- 기본 한 tick은 게임 시간 1초이며 매 tick 배고픔이 0.02 증가합니다. 식량과 배고픔 조건에 따라 먹습니다.
- 굶주리고 돈이 부족한 시민은 도적으로 바뀝니다. 같은 지역의 도적은 시민의 돈을 최대 2 약탈하며 범죄 횟수와 지역 위협도가 증가합니다.
- 식량이 부족한 NPC와 판매 가능한 NPC 사이에서 1골드/1식량 거래가 일어납니다. 두 자원의 합은 보존됩니다. 돈·식량의 외부 생산은 기존 경제 시스템에서 설계해야 합니다.
- 비도적 NPC의 반복 만남은 친밀도를 쌓고 10회에 Love 이벤트를 한 번 냅니다. 결혼·가족·성인 여부 등의 세계관 규칙은 게임 측 관계 시스템에서 판단해야 합니다. 여기서는 관계 형성 신호입니다.
- NPC 살인은 기존 전투 결과를 `kill()`로 전달할 때 기록합니다. 일반 NPC가 NPC를 죽이면 도적화하고 위협도가 증가합니다. 기사/마법사의 NPC 처치는 현재 정당한 진압으로 취급하는 단순 정책이므로 진영·목격자·정당방위 규칙이 필요하면 이 조건을 확장하세요.
- 지역 위협도는 tick마다 0.1 감소하고 3 이상이면 가용 기사 또는 마법사 1명을 파견합니다. 같은 지역에 중복 파견하지 않고 위협이 줄면 귀환합니다. 실제 진압 전투는 기존 게임이 수행합니다. 거주지에서 떨어져도 다른 지역의 첫 번째 가용 수비자를 선택하므로 실제 정치·거리 정책은 교체해야 합니다.
- 마수는 평소 home을 유지하고 hunger 0.8 이상 또는 `lure(id,true)` 시 침공합니다. 기본 목적지는 home 이외의 가장 작은 지역 ID입니다. 이것은 연결 예제용 정책입니다. 실제 지형 인접성, 거리, 먹이, 유인 위치는 별도 목적지 선택 정책으로 확장하세요. 굶주림이 줄고 유인이 해제되면 귀환합니다.
- 침공 중에는 지역 위협도가 올라갑니다. 마수 AI의 세밀한 이동·먹이 사냥·전투는 기존 엔진에 위임합니다.

마법사가 마수를 처치할 때 기본 마력 보상:

```text
r = 마수 전투력 / 마법사 전투력
효율 = r < 1 이면 r / (1 + 같은 마수 종류의 기존 약체 처치 횟수), 아니면 1
획득 마력 = manaScale × 마수 전투력 × 효율
manaScale 기본값 = 0.1
```

자신보다 강한 마수의 보상은 전투력에 비례합니다. 약한 마수는 상대 전투력과 종류별 반복 횟수로 보상이 줄어듭니다. 예를 들어 마법사 전투력 10, 마수 전투력 5일 때 첫 보상은 0.25, 두 번째는 0.125입니다. 전투력 20인 마수는 2를 줍니다. 증가분은 `mana`이며 전투력 `power`의 증가로 자동 환산하지 않습니다. 마력과 전투력의 관계는 기존 성장 시스템에서 정해야 합니다.

## 5. 39개 WBG 구현 절차

1. 원본 WBG의 ID·설명·발동 조건·효과·우선순위를 확보합니다.
2. 매핑으로 원문을 읽고 실제 ID 39개를 대조합니다.
3. 원본 ID마다 `WbgHandler`를 구현해 `std::map<std::string,WbgHandler>`에 등록합니다.
4. 핸들러는 정렬된 ID 순서로 매 tick, 기본 행동 이후 호출됩니다. 원래 설정에 우선순위가 있다면 등록/호출 정책을 명시적으로 확장하세요.
5. 핸들러는 const Snapshot을 읽고 출력 이벤트 또는 게임별 명령 큐를 생성합니다. 콜백에서 World를 재진입·직접 변경하지 마세요. 콜백은 예외를 던지지 않도록 작성해야 합니다. 예외 시 tick 전체 롤백을 제공하지 않습니다.
6. 효과로 상태 변경이 필요하면 게임의 명령 큐를 tick 이후 적용하는 공개 API를 추가합니다. 현재 기본 API는 `lure`, `kill`, 전체 `restore`이며 임의 명령/스크립트 해석기는 포함되어 있지 않습니다. 매번 restore하면 대기 이벤트가 지워지므로 일반 명령 적용에 쓰지 마세요.
7. 원본 규칙과 기본 행동이 같은 효과를 다룰 경우 기본 행동의 해당 분기를 게임 정책으로 교체하여 중복 보상을 막습니다.

## 6. 저장, 프레임, 스레드, 규모

원본 콘텐츠 DB를 세이브 DB로 사용하지 않습니다. 기존 저장 시스템에서 `Snapshot`을 직렬화하고 불러올 때 `restore()`합니다. tick, accumulator, 전체 Actor(종류별 weakKills·alive·lured·crimes 포함), Region 위협도, deployments, affinity를 모두 저장해야 반복 보상·관계·파견이 초기화되지 않습니다. 별도의 파일 포맷/SQLite 세이브 저장기는 포함되어 있지 않습니다. 게임 버전·콘텐츠 버전·Config를 기존 세이브 헤더에 함께 기록하고 같은 Config로 복원하세요. 보이지 않는 대기 이벤트는 저장되지 않으므로 이벤트를 반영한 프레임 경계에서 저장합니다.

고정 tick은 렌더링 FPS와 분리됩니다. 한 frame에서 최대 8 tick을 처리하며 이를 초과하는 경과 시간은 버립니다. 로딩/최소화 후 급격한 몰아 실행을 막는 기본 정책입니다. 오프라인 수 시간 진행을 원하면 별도 제한된 배치 처리 정책을 구현하세요. 실행 중인 동일 World는 한 스레드만 소유해야 합니다. 렌더링 스레드에는 `Snapshot` 복사본을 게시하고 원본 참조를 동시에 읽고 쓰지 않습니다.

고정 ID 순서와 난수 없는 기본 행동으로 같은 입력과 tick 순서는 같은 진행을 만듭니다. 임의 WBG 콜백의 외부 상태·난수·실수 플랫폼 차이까지 보장하지는 않습니다. 기본 상호작용 검색은 최악 O(N²)이므로 대규모 월드는 지역별 인덱스와 활성/비활성 구역 LOD가 필요합니다. 대규모 성능 측정은 이 패키지의 검증 범위 밖입니다.

## 7. 통합 완료에 필요한 자료

실제 `blood_principle_game.db`, 39개 WBG 원문 또는 해당 테이블 덤프, 기존 게임의 업데이트/전투 확정/엔티티 ID 코드와 빌드 설정이 있으면 매핑·각 WBG 처리 함수·정확한 호출 위치를 확정할 수 있습니다. 현재 결과는 바로 빌드 가능한 독립 모듈과 연결 계약이며 원본 게임에 실제 적용됐다는 뜻은 아닙니다.

## 공식 문서

- SQLite 읽기 전용 연결: https://www.sqlite.org/c3ref/open.html
- CMake SQLite 찾기: https://cmake.org/cmake/help/latest/module/FindSQLite3.html
- SQLite 공식 소스 다운로드: https://www.sqlite.org/download.html
