# 로컬 월드 모니터 (World Watch)

월드 통합 방식에 의존하지 않는 읽기 전용 모니터입니다. 각 게임이 JSON 전체 스냅샷을 보내고, 로컬 수신기가 마지막 상태를 보관하며, HTML 화면에서 월드와 마을을 선택해 확인합니다. 외부 서비스 등록, 배포, npm 패키지가 필요하지 않습니다.

## 시작

1. Python 3.10 이상이 설치된 PC에서 프로젝트 루트를 기준으로 실행합니다.

   ```powershell
   python monitor/server.py
   ```

2. 브라우저에서 **http://127.0.0.1:8765** 를 엽니다. HTML 파일을 직접 더블클릭하지 않습니다.
3. 변경된 C++ 프로젝트를 빌드한 뒤 게임을 실행합니다. 약 1초마다 자동으로 전송합니다. 도움말·대화·스탯 화면으로 게임이 멈춰도 스냅샷은 전송됩니다.
4. 수신기는 Ctrl+C로 종료합니다. 게임과 수신기는 어느 순서로 실행해도 됩니다. 포트가 사용 중이면 기존 수신기를 사용하거나 해당 프로세스를 확인하세요.

게임 실행 옵션:

```text
SimpleGame.exe --arcana
SimpleGame.exe --lena
SimpleGame.exe --village
SimpleGame.exe --level-one --world-id=ash-region
SimpleGame.exe --no-monitor
```

기본 실행은 ARCANA이며 world_id는 `WORLD_ARCANA_BELHEIM`, 마을 ID는 `belheim`입니다. 레나는 `--lena`와 `WORLD_AERIA_LENA`, 기존 마을 모드는 `village`, 레벨 1은 `level-one`입니다. `--world-id`는 1~64자의 영문자·숫자·밑줄·점·하이픈을 허용합니다. `--no-profile`은 성능 로그만 끄고 모니터는 유지합니다. `--no-monitor`는 캡처와 전송 모두 끕니다.

ARCANA 개체 JSON에는 `mana_current`, `mana_max`, `is_beast`, `mana_absorbed`가 포함됩니다. 월드 지표의 `arcana.quest_step`, `arcana.territory_disturbance`, `arcana.knight_dispatched`로 진행과 사건을 확인할 수 있습니다. 원격 도시는 현재 시작 맵의 실제 배치 개체 수에 포함하지 않습니다.

## 표시 내용과 의미

- 월드/실행 인스턴스 선택, 수신 상태, 일시정지, 월드 시간.
- NPC, 몹, 야생동물, 플레이어의 **생존 수 / 전체 수**. 산적은 적대적이어도 NPC로 집계됩니다. 사망·재생성 대기 몹은 전체 수에만 포함됩니다.
- 마을별 소속 개체 수, 체력 25% 이하인 생존 NPC, 적대 NPC, 항복 NPC.
- NPC 행동(WBG 코드와 한글 설명), 체력, 위치, 대상 ID, 활성·표시 플래그, 배고픔·갈증·피로, 소지품, 7가지 성격 지표.
- 이름·역할·행동 검색, 종류·마을·생존 상태 필터, 50개씩 페이지 표시, 개체 원본 JSON.
- resident_* 지표는 **주민 개인 보유량 합계**이며 창고 재고가 아닙니다. 레나 모드는 별도로 실제 창고 식량·사기·치안·오염을 전송합니다. 해당 지표를 보내지 않는 기존 모드나 미구현 경제 종합 지수는 ‘미수집’입니다.
- 현재 마을 주변 몹은 해당 마을의 관리 소속으로 보내며 실제 마을 경계 안에 있다는 뜻이 아닙니다. 레벨 1의 야영지는 마을 항목으로 묶고, 실제 NPC가 없으므로 NPC 0명입니다.
- Actor의 visible은 자체 표시 플래그이며 카메라에 보이는지 또는 부모가 숨겨졌는지를 나타내지 않습니다.
- 미계측 값은 null/‘미수집’. 야생동물 체력과 레벨 1의 세부 AI 행동 등은 임의로 만들어내지 않습니다.

수신 시각 기준 **5초 초과 미수신**은 ‘오래된 정보’로 표시합니다. 수신기가 끊겨도 브라우저는 마지막 데이터를 남기지만 신선한 정보로 표시하지 않습니다. 마지막 데이터는 서버에서 최대 1시간 보관하고 이후 제외합니다. 서버 재시작 시 메모리 상태는 사라집니다. 이력 저장·알림·게임 제어 기능은 포함하지 않습니다.

## 다른 게임 연결: JSON v1

`POST http://127.0.0.1:8765/api/snapshots` 에 UTF-8 `application/json`으로 전송합니다. `Content-Length`가 필요하며 chunked 전송은 받지 않습니다. [예제](examples/village.json)는 형식 확인용 가상 데이터입니다. 운영 게임의 상태를 흉내 내지 않도록 `game_id=demo`를 사용합니다.

```powershell
curl.exe -i -X POST http://127.0.0.1:8765/api/snapshots -H "Content-Type: application/json" --data-binary "@monitor/examples/village.json"
curl.exe -i -X POST http://127.0.0.1:8765/api/snapshots -H "Content-Type: application/json" --data-binary "@monitor/examples/forest.json"
```

동일 예제를 다시 보내려면 sequence를 증가시키거나 새로운 instance_id를 사용해야 합니다. 단발 예제는 5초 후 오래된 정보로 전환되는 것이 정상입니다.

| 필드 | 의미 |
| --- | --- |
| schema / type | `world-monitor.v1` / `snapshot` 고정 |
| game_id / world_id / instance_id | 게임 종류 / 논리 월드 / 실행 인스턴스. 세 값으로 스트림 구분 |
| sequence | 인스턴스 수명 동안 단조 증가하는 정수. epoch 변경 시에도 초기화하지 않음 |
| epoch | 씬 재생성 세대. 같은 스트림에서 감소 불가. Actor ID 재사용 시 증가 |
| paused | 게임 진행 일시정지 여부, boolean |
| world_time_minutes | 게임 내 시간(분), 모르면 null 또는 생략 |
| bounds | min_x, min_y, max_x, max_y. 최소 < 최대, 월드 좌표 경계 |
| metrics | 월드 단위 숫자/null 지표 사전 (선택) |
| settlements | id, name, 선택 metrics를 가진 마을 배열 |
| entities | 아래 개체 구조의 배열 |

개체 필드:

| 필드 | 의미 |
| --- | --- |
| id / name | 스트림·epoch 내부 유일 ID / 표시 이름 |
| kind | `npc`, `monster`, `wildlife`, `player` 중 하나 |
| settlement_id | settlements에 존재하는 ID, 미소속은 null/생략 |
| role / action / action_label | 역할 코드 / 행동 코드 / 표시용 행동 설명 |
| target_id | 관측 대상 ID. 비관측 Actor일 수 있으므로 참조 해석 불가 허용. 없으면 null/생략 |
| position | x, y 숫자. 화면 좌표가 아닌 월드 좌표 |
| health / max_health | 비음수 숫자 또는 null/생략 |
| alive / active / visible / hostile | boolean. 생존과 적대 여부는 송신 게임이 판정 |
| metrics / personality | 선택적인 지표 사전, 값은 숫자 또는 null |

ID와 지표 키는 1~64자 ASCII 영숫자·밑줄·점·하이픈, 표시 문자열은 256자 이하입니다. 지표 사전은 최대 64개. 숫자는 유한하며 절댓값 1e15 이하, sequence/epoch는 0..2^53-1 정수입니다. bool은 숫자로 받지 않습니다. 추가 필드는 v1 핵심 필드를 바꾸지 않는 범위에서 보관하지만 UI는 핵심 필드만 해석합니다. 누락 가능한 필드 외에는 필수입니다.

**스냅샷은 전체 교체입니다.** 같은 월드의 이전 스냅샷에서만 존재하던 개체는 제거됩니다. 차분 메시지나 여러 청크의 부분 목록을 같은 스트림에 보내면 안 됩니다. 여러 게임의 NPC ID가 같아도 스트림이 다르면 충돌하지 않습니다. 각 게임은 프로세스 재시작 시 새 instance_id를 발급해야 합니다.

- 응답 202: 수락, 400: 잘못된 JSON/규격, 403: 외부 origin/Host, 409: 오래된 sequence/epoch, 413: 크기 초과, 415: 잘못된 Content-Type, 429: 용량 초과.
- 한 메시지 1 MiB, 한 스냅샷 개체 2,000개·마을 128개, 수신 스트림 32개, 보관 원본 크기 합 16 MiB 제한. 파싱 후 객체 메모리는 원본보다 큽니다. 동시 HTTP 처리 8개와 요청 소켓 timeout 3초를 적용합니다.
- 같은 논리 월드의 여러 인스턴스는 별도로 표시합니다. 서버는 월드 간 이동이나 플레이어 중복을 추론하지 않습니다. 이동 통합 방식이 정해지면 전역 entity ID 및 소유권 이전 이벤트를 별도 버전으로 설계합니다.

## 구현과 성능

`GameMode::CaptureMonitor()`는 게임 스레드에서 SceneGraph의 Actor ID를 확인하고 데이터만 복사합니다. `WorldMonitor`는 JSON을 직렬화한 뒤 별도 스레드가 WinHTTP로 전송합니다. 대기 메시지는 최신 1개로 제한합니다. 수신기 장애 시 2초 재시도 간격과 네트워크 단계별 250ms timeout을 사용합니다. 종료 시 전송 스레드의 종료를 기다립니다. 네트워크 대기는 렌더링 스레드에서 하지 않지만 캡처·직렬화 CPU 비용은 존재하므로 프로파일링 값으로 확인하세요.

수신 주소는 127.0.0.1:8765로 고정, 프록시·리다이렉트 없음. HTTP 서버는 loopback에만 바인딩하고 Host/Origin을 검증합니다. CORS 허용 없음, 외부 스크립트 없음, 문자열은 textContent로 출력합니다. **같은 PC의 프로세스는 신뢰하는 개발용 도구**이며 인증된 운영 서비스가 아닙니다. 포트 포워딩이나 0.0.0.0 바인딩으로 공개하지 마세요.

## 사용자 검증 절차

이 변경에서는 빌드·서버 실행·화면 검증을 대신 수행하지 않았습니다.

```powershell
python -m unittest discover -s monitor/tests -v
```

1. 수신기 없이 게임 실행: 게임 진행 가능, `monitor.send_failures` 증가 확인.
2. 수신기 시작 후 자동 수신, NPC·몹 수가 실제 게임 상태와 일치하는지 확인.
3. 공격·대화·식사·항복·몹 사망 전후 상세 상태 비교. 도움말 중 paused 표시 확인.
4. 서로 다른 `--world-id`로 두 게임을 실행하여 월드가 구분되는지 확인.
5. 게임 재시작, 씬 재생성(R/N), 수신기 재시작 후 ID/epoch와 데이터 교체 확인.
6. 게임 종료 5초 후 오래된 정보 표시, 수신기 종료 시 연결 끊김 표시 확인.
7. 동일 sequence 재전송 409, malformed JSON 400, 잘못된 Origin 403 확인.
8. `--no-monitor`와 기본 실행의 `monitor.capture`, `monitor.serialize`, FPS 로그를 비교해 관측 비용 확인.
