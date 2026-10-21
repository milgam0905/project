# 혈통의 법칙: 기존 OpenGL/C++ 프로젝트용 DB 통합 모듈 v1.1

원본 `blood_principle_game.db` v1.0.0에 맞춘 C++17 독립 모듈입니다. 기존 게임의 실행 파일·렌더러·창·입력·전투·프로젝트 구성을 유지하고 소스 또는 정적 라이브러리만 추가합니다.

이번 버전의 통합 진입점은 **`bp::native`**입니다. 이전 버전의 `bp::sim`, 범용 `loadContent()`와 `mapping/*.sql`은 원본을 확인하기 전의 프로토타입으로 남겨두었습니다. 원본 DB 연결에는 사용하지 마세요. 기존 프로토타입 가이드는 `docs/PROTOTYPE_GUIDE.ko.md`로 옮겼습니다.

## 실제 DB에서 확인한 내용

| 항목 | 결과 |
|---|---|
| 정적·세이브 스키마 | 40개 테이블, SQLite 무결성 정상, 외래키 오류 없음 |
| WBG | 별도 39개 규칙 레코드가 아닌 `behavior_nodes`의 39개 행동 노드 |
| 프로필 | human, herbivore, predator, beast; `profile_nodes` 156행에 사용 가능 여부·기본 가중치 보관 |
| 초기 세이브 | `demo`, 개체 9개: 인간 7·마수 1·일반 동물 1 |
| ID | entity/species/region/save/profile 등 TEXT ID를 그대로 유지 |
| 영역 | 초기 마수 영역 1개, `territories`에 거주 지역과 교란 수치 보관 |
| 욕구 수치 | 원본 침공 조건은 hunger > 75, disturbance > 80; 어댑터는 욕구를 0~100으로 다룸 |
| 마력 | `mana_current`와 `mana_max`를 구분; 흡수는 최대 마력만 증가 |

감사 결과와 실제 노드 목록은 `DB_AUDIT.json`, `docs/WBG_ADAPTERS.ko.md`에서 확인할 수 있습니다. 배포 폴더의 `data/blood_principle_game.db`는 사용자 원본과 SHA-256이 동일한 복사본입니다.

첨부 `build.py`는 원본 재생성 및 삭제 동작이 있어 실행하지 않았습니다. 첨부 SQL 예제의 UPDATE/INSERT도 원본에 실행하지 않았습니다. 첨부 자료는 데이터 구조와 게임 설정을 파악하는 참고 자료로 사용했습니다.

## 파일과 구현 범위

| 파일/기능 | 제공 내용 |
|---|---|
| `include/bp/native.hpp`, `src/native.cpp` | 실제 DB 읽기 전용 로더, WBG 선택기, 침공·파견·상호 관계 조건 판정, 마력 계산·저장 |
| `examples/native_main.cpp` | 원본 DB로 실행 가능한 로드/행동 선택 예제 |
| `examples/host_integration.cpp.txt` | 기존 게임 초기화·AI 업데이트·전투 결과에 넣을 연결 골격 |
| `tools/prepare_runtime.py` | 원본을 덮어쓰지 않는 실행용 DB 복사 도구 |
| `RuntimeStore` | 마수 사망·최대 마력 흡수·동일 종류 사냥 횟수·현재 행동 저장 |
| `tests/native_tests.cpp` | 실제 DB 로드, 행동 유지·전환·긴급 중단, 조건 검사, 보상·로그 원자성 테스트 |

행동 선택 코드는 39개 실제 노드 전체를 인식하고 각 종의 enabled/base_weight를 적용합니다. 각 노드의 유효한 대상·효용 입력과 실제 이동/공격/대화 실행은 기존 엔진의 Host 어댑터로 연결해야 합니다. 예제 Host는 읽기 전용 확인을 위해 Idle/Wander만 지원합니다. 39개 행동의 물리·전투·대화까지 완성된 게임 엔진은 아닙니다.

관계는 양방향 행을 따로 읽고 동의·수치 조건을 판정합니다. NPC 거래/살인/약탈의 실행, 신고 생성, 예산 소모, 출생·상속·결혼 데이터 변경, 인구 단위 LOD는 기존 게임 이벤트 처리기에 연결해야 합니다. 이 버전이 해당 도메인을 자동으로 모두 변경하지는 않습니다.

## 빌드: 기존 프로젝트는 유지

CMake를 쓰는 게임에서는 이 폴더를 원하는 위치에 추가한 후 기존 타깃에 연결합니다. YourGame은 원래 실행 파일 타깃 이름으로 바꿉니다.

```cmake
add_subdirectory(external/blood_principle_module)
target_link_libraries(YourGame PRIVATE bp::native)
```

모듈은 C++17과 SQLite가 필요하며 OpenGL/GLFW/SDL을 직접 참조하지 않습니다. 설치된 SQLite는 `find_package(SQLite3)`로 찾습니다. 프로젝트가 이미 SQLite를 관리한다면 `src/native.cpp`만 기존 타깃에 추가하고 모듈의 `include`와 기존 SQLite 헤더/라이브러리를 연결해도 됩니다. 이 경우 이전 프로토타입 소스는 필요하지 않습니다. SQLite를 두 번 컴파일/링크하지 마세요.

CMake를 사용하지 않는 Visual Studio 프로젝트도 `native.cpp`, include 경로, C++17, 기존 SQLite 링크만 추가하면 됩니다. Windows는 `/utf-8`과 동일한 x64/x86·CRT 설정을 권장합니다. UTF-8 경로 문자열을 전달하세요.

모듈 폴더에서 별도 빌드·검증:

```powershell
cmake -S . -B build -A x64 -DBP_BUILD_TESTS=ON -DBP_BUILD_EXAMPLE=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
./build/Release/bp_native_example.exe ./data/blood_principle_game.db demo
```

SQLite 라이브러리를 찾지 못하면 다음 둘 중 하나를 사용합니다.

```powershell
cmake -S . -B build -A x64 -DSQLite3_INCLUDE_DIR="D:/deps/sqlite/include" -DSQLite3_LIBRARY="D:/deps/sqlite/lib/sqlite3.lib" -DBP_BUILD_TESTS=ON -DBP_BUILD_EXAMPLE=ON
```

```powershell
cmake -S . -B build -A x64 -DBP_SQLITE_SOURCE_DIR="D:/deps/sqlite-amalgamation" -DBP_BUILD_TESTS=ON -DBP_BUILD_EXAMPLE=ON
```

후자의 폴더에는 공식 `sqlite3.c`, `sqlite3.h`가 있어야 합니다. 모듈이 C 소스로 한 번 컴파일합니다. SQLite JSON 함수와 UPSERT가 필요하므로 JSON을 포함한 SQLite 3.38 이상을 사용하세요. 검증 환경은 Windows/MSVC이며 Linux/macOS에서는 `-A x64`를 빼고 별도 실행 확인이 필요합니다.

## 원본 콘텐츠와 실행용 저장소 분리

원본을 읽기 전용으로 로드할 수 있습니다.

```cpp
auto content = bp::native::load("assets/blood_principle_game.db", "demo");
```

게임 진행을 저장하려면 먼저 별도의 실행용 DB를 만듭니다. 경로가 이미 있으면 복사 도구가 거부하므로 다른 경로를 선택하세요. 원본 파일은 그대로 둡니다.

```powershell
python tools/prepare_runtime.py data/blood_principle_game.db runtime.db
```

복사본에는 기존 스키마에 `bp_runtime_meta` 표식과 중복 흡수 방지 인덱스만 추가됩니다. `RuntimeStore`는 표식이 없는 원본 DB에 쓰기를 거부합니다. 기존 게임 DB를 이 DB로 덮어쓰지 마세요. 원래 저장 시스템과 병행하는 보조 DB입니다.

`demo`를 통째로 복사한 독립 실행 파일이므로 파일별로 저장 슬롯을 구분할 수 있습니다. 같은 DB 안에 새 save ID를 만드는 복제/마이그레이션 기능은 제공하지 않습니다. 이를 구현할 때 전역 PK인 entities.id를 포함한 모든 참조를 새 ID로 일관되게 바꿔야 합니다.

## 기존 게임 루프 연결

1. 시작 시 runtime.db 또는 원본에서 Content를 읽고 `Scheduler(content)`를 만듭니다.
2. 문자열 entity ID와 기존 엔티티 핸들 사이의 대응표를 유지합니다. `player`는 기존 입력 시스템이 제어하므로 일반 NPC AI에서 제외합니다.
3. 프레임 업데이트에서 게임 시간 초 단위 `nowSeconds`를 전달합니다. 기본 기준은 `content.worldHours * 3600`입니다. NPC 상태는 게임에서 최신화한 Entity를 넘깁니다. 로드 당시의 복사본을 계속 쓰면 욕구·지역 등이 갱신되지 않습니다.
4. Host::probe에서 각 활성 노드의 유효한 대상과 효용 항을 제공하고, Host::execute에서 기존 AI/이동/전투/대화 기능을 호출합니다. `Intent.changed`가 false면 이미 실행 중인 동작을 이어가고 새 이동/공격을 중복 시작하지 않습니다.
5. 저장 시 `store.saveActions(save, nowSeconds/3600, scheduler.actions())`를 호출합니다. 현재 행동의 시작·최소 유지·쿨다운 시간은 DB의 시간 단위로 변환해 저장합니다.

Content/Scheduler는 엔티티 좌표나 욕구를 스스로 매 프레임 증가시키지 않습니다. 시간 경과와 자원 소비는 기존 엔진이 소유하고, 어댑터는 선택과 저장을 담당합니다. `saveActions`는 행동과 세계 시간만 저장하며 위치·인벤토리·관계 등 전체 상태의 저장 기능은 아닙니다. 이 필드는 기존 게임 저장 계층에서 해당 테이블에 기록하세요.

## WBG 선택 규칙

```text
utility = profile_nodes.base_weight
          + need + emotion + relationship + environment
          + personality + memory + goal + random
```

DB에 weight_terms 목록은 있으나 세부 가중치/점수 공식은 없습니다. 위 덧셈은 이번 C++ 어댑터의 명시적 초기 정책이며 각 항은 Host가 같은 척도로 제공합니다. random 항도 Host가 세이브 seed와 함께 생성해야 재현할 수 있습니다.

- 프로필에서 disabled인 노드는 평가하지 않습니다.
- Host가 nullopt 또는 valid=false를 반환한 행동은 제외합니다.
- requiresTarget=true이고 DB가 require_valid_target를 켠 경우 빈 target은 제외합니다. 대상의 실제 생존·도달·시야 등은 Host에서 검사합니다.
- 최초 최고 점수를 선택하며 동점은 작은 노드 ID가 우선합니다.
- 기존 행동이 아직 유효하면 최소 2초를 유지합니다.
- 이후 새 행동이 기존 행동보다 0.15를 초과해서 좋아야 전환합니다. DB는 hysteresis의 상세 산식이 없으므로 이 버전은 절대 점수 차로 정의합니다.
- 치명적 위협/화재/긴급 보호 Interrupt는 유지 시간·전환 억제를 우회합니다. 어떤 행동이 위협에 적합한지는 Host 점수가 결정합니다.
- 기존 행동이 무효화되면 유지 시간 중에도 재선택합니다.
- 후보가 전혀 없으면 현재 행동을 비우고 nullopt를 반환합니다. Host에서 Idle fallback을 제공하는 것을 권장합니다.

`active_actions.target_id`는 엔티티 FK이므로 저장 가능한 target은 같은 save의 entity ID입니다. 좌표·아이템·정착지 대상은 엔진 측 행동 컨텍스트에 별도로 저장하고 target을 임의 엔티티 ID로 위장하지 마세요. 현재 adapter는 context_json 전체 직렬화를 제공하지 않습니다.

## 자율 사건 조건

마수: `shouldLeaveTerritory()`는 마수이며 살아 있고, 배고픔 > 75 또는 교란 > 80 또는 유인·영역 상실·번식 조건을 충족하면 true입니다. 다른 모든 경우에는 영역 유지 의도로 처리하세요. 주어진 Territory는 해당 마수의 영역이어야 합니다. 실제 목적지와 이동 경로는 기존 내비게이션에서 정합니다. 이 버전은 인접성 없는 임의 지역 이동을 하지 않습니다.

토벌: `shouldDispatch()`는 신고된 미해결 범죄 ≥ 3, 치안 < 40, 가용 기사 ≥ 1 조건을 사용합니다. 이는 원본 dispatch_knights 템플릿의 조건입니다. 마법사 동행 여부·예산·출발 위치·도적 추적은 기존 파견 처리기에서 정하고 이미 진행 중인 토벌은 중복 생성하지 마세요. 범죄 기록이나 위협 사건이 곧바로 순간이동·자동 사망으로 이어지지 않습니다.

관계: `canDate()`는 반대 방향의 두 관계 행에서 양쪽 affection ≥ 65, attraction ≥ 50, 상호 동의를 요구합니다. `canMarry()`는 양쪽 commitment ≥ 75, 상호 동의와 혼인 자격을 요구합니다. 동의·자격은 엔진의 검증 결과를 전달해야 합니다. 조건 충족만으로 DB 행을 자동 생성하지 않으며 실제 사건 처리기가 관계·계약·가족 링크를 변경합니다.

## 마수 사망과 마력 증가

기존 전투 시스템이 마수 사망을 확정했을 때:

```cpp
bp::native::RuntimeStore store("runtime.db");
store.recordDeath("demo", beastId, worldHours);

bp::native::Eligibility proof;
proof.mageBloodline = verifiedBloodline;
proof.eligibleContributor = verifiedCombatCredit;
proof.inAbsorptionRange = verifiedRange;
double gain = store.absorb("demo", mageId, beastId, worldHours, proof);
```

혈통·기여도·거리의 진위는 기존 엔진이 책임집니다. 현재 DB에는 확정 혈통 boolean이나 전투 기여도 원장이 없으므로 직업명·근처에 있었다는 이유만으로 자격을 추정하지 않습니다. `recordDeath()`는 entities.alive를 0으로 바꾸고 사망 시 마수의 mana_max를 world_events의 payload에 보존합니다. 엔진에서 먼저 DB의 alive를 0으로 바꾸지 말고 이 API로 통합하거나, 동일한 사망 이벤트 기록을 갖춘 별도 어댑터를 구현하세요.

```text
relative = min(1.5, (beast_mana / max(1, mage_mana_max))^1.3)
repetition = 1 / (1 + 0.12 * previous_same_species_kills)
gain = beast_mana * 0.25 * relative * repetition
```

모든 상수와 60초 흡수 제한은 `simulation_rules.mana`에서 읽습니다. 이 어댑터는 beast_mana를 사망 시 개체 mana_max로 정의합니다. 종류별 `base_mana_yield`는 스폰 기본값 설정에 활용할 수 있지만 개체 최대 마력을 덮어쓰지 않습니다.

`absorb()`는 최대 마력만 증가시키며 현재 마력은 그대로 둡니다. 동일 마수·동일 마법사의 재요청은 0을 반환합니다. 여러 기여자에게 각각 지급할지 여부는 원본 credit_rule의 세부 배분 규칙이 없으므로 각 호출의 엔진 자격 검증에 맡기며, 전체 보상을 참여자 사이에서 자동 분할하지 않습니다. 종류별 사냥 횟수는 강약 구분 없이 원본 공식대로 반영합니다.

`BEGIN IMMEDIATE` 한 트랜잭션에서 최대 마력·사냥 기록·흡수 로그를 함께 갱신하며 로그 쓰기 실패 시 모두 취소합니다. 재시작 후에도 중복 흡수 방지가 유지됩니다. 사망과 흡수는 별도 트랜잭션이므로 사망 직후 게임을 저장해도 60초 안에 기여자가 다시 흡수할 수 있습니다. 시간이 뒤로 가는 쓰기는 거부합니다.

과학 지식에 의한 효율 향상은 최대 마력 증가와 분리해야 합니다. 원본의 `spell_knowledge.efficiency_multiplier`와 환경 조건은 기존 주문 처리기에 연결하며 이 모듈이 지식 발견만으로 최대 마력을 늘리지는 않습니다.

## 스레드·오류·LOD

Scheduler와 엔티티 갱신은 게임의 AI 소유 스레드에서 수행하고 렌더 스레드에는 복사한 표시 상태를 넘깁니다. Host::probe는 읽기 평가만 하고 execute에서 명령을 발행하세요. 콜백 재진입·예외 롤백은 지원하지 않으므로 Host의 예외 정책을 정해야 합니다. RuntimeStore는 호출당 연결을 열고 2초 busy timeout을 사용하며 실패는 예외로 반환합니다.

`rulesJson`에는 원본 6개 설정 원문을 유지합니다. 그중 WBG/마력 및 침공·파견 템플릿 일부를 실행에 연결했습니다. LOD의 near/full, mid/abstract, far/population 선언은 읽지만 전체 인구 생태계·온로드 병합 코드는 아직 없습니다. 기존 엔진의 활성화/거리 정책에서 named NPC 지속성과 집계 복원을 구현해야 합니다. DB가 선언한 모든 JSON을 자동 해석하는 범용 스크립트 엔진은 포함하지 않습니다.

## 검증과 남은 연결

실제 원본 DB에 대해 39개 노드·4개 프로필·9개 개체 로드 및 네이티브 실행 예제를 확인했습니다. 빌드·테스트 결과는 `VALIDATION.ko.md`에 기록했습니다. 원본과 배포 복사본의 해시도 대조했습니다.

현재 작업 공간에 기존 OpenGL 게임 소스는 없으므로 실제 게임의 AI·NavMesh·전투·저장 호출 위치에 적용하는 작업은 아직 수행하지 않았습니다. 기존 프로젝트의 빌드 파일과 AI 업데이트/전투 확정 코드를 제공하면 위 Host 어댑터의 구체 호출부를 맞출 수 있습니다.
