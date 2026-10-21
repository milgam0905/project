# 원본 39개 WBG 행동 노드와 엔진 연결표

Scheduler는 아래 모든 노드의 원본 ID와 프로필 가중치를 읽습니다. 표의 실행 기능은 기존 엔진의 Host에 연결합니다. 본 예제 Host는 Idle/Wander만 실행 의도로 출력합니다. 나머지 노드를 구현된 것으로 가장해 빈 처리기로 등록하지 마세요.

| ID | 노드 | 활성 프로필 | Host 연결 책임 |
|---|---|---|---|
| 1 | Evaluate | beast, herbivore, human, predator | 주변 상태·욕구·위험을 평가하는 기존 감지/AI 서비스 |
| 2 | Observe | beast, herbivore, human, predator | 시야·소리·환경 관찰 |
| 3 | Investigate | beast, herbivore, human, predator | 의심 지점·단서 조사 및 도달성 확인 |
| 4 | Remember | beast, herbivore, human, predator | 기억 생성·회상; 사실 로그와 NPC 기억 구분 |
| 5 | Decide | beast, herbivore, human, predator | 상위 목표 확정; 재진입 없이 다음 명령 큐에 반영 |
| 6 | Idle | beast, herbivore, human, predator | 대기 애니메이션·안전한 fallback |
| 7 | Wander | beast, herbivore, human, predator | 영역 안의 도달 가능한 배회 위치 선택 |
| 8 | Travel | beast, herbivore, human, predator | 기존 경로 탐색·목적지 이동 |
| 9 | Sleep | beast, herbivore, human, predator | 수면 장소·시간·위협 검사 및 피로 감소 |
| 10 | Eat | beast, herbivore, human, predator | 실제 음식 재고 확인·소비·배고픔 감소 |
| 11 | Drink | beast, herbivore, human, predator | 식수 접근·소비·갈증 감소 |
| 12 | Work | human | 직업·작업장·일정·보상 |
| 13 | Rest | beast, herbivore, human, predator | 안전한 휴식과 피로 회복 |
| 14 | Talk | human | 대화 대상·관계·지식 공유·대화 UI |
| 15 | Greet | human | 인지된 상대에게 일회성 인사 |
| 16 | Trade | human | 재고·자금 검증과 자원 이전의 단일 트랜잭션 |
| 17 | Help | human | 도움 요청·자격·자원·수락 |
| 18 | Follow | human | 추종 대상·거리·경로 유지 |
| 19 | Avoid | beast, herbivore, human, predator | 회피 대상과 안전한 경로 |
| 20 | Warn | beast, herbivore, human | 위협 전달 및 수신자의 주관적 기억 |
| 21 | CallHelp | herbivore, human | 지원 요청·듣는 NPC·세력 반응 |
| 22 | Argue | human | 갈등 대상·분노·관계 변화 |
| 23 | Threaten | human | 위협 발언·반응·범죄 여부 |
| 24 | Intimidate | human | 위압과 상대의 공포·사기 |
| 25 | Steal | human | 절도 대상·목격자·재산 이전·crimes 기록 |
| 26 | Chase | beast, herbivore, human, predator | 추적 대상의 최근 위치·경로 |
| 27 | Surrender | human | 항복 가능성·상대 수락·전투 종료 |
| 28 | Attack | beast, human, predator | 기존 전투의 사거리·공격·피해 확정 |
| 29 | Defend | beast, human, predator | 방어 상태·스태미나·피해 감소 |
| 30 | Dodge | beast, human, predator | 회피 공간·쿨다운·스태미나 |
| 31 | Flee | beast, herbivore, human, predator | 안전 지점·도주 경로·추격 반응 |
| 32 | SeekCover | beast, human | 엄폐물 선택·점유·경로 |
| 33 | Protect | beast, herbivore, human, predator | 보호 대상·적대 관계·긴급 보호 우선순위 |
| 34 | Hunt | beast, human, predator | 먹이·마수 목표·사냥 계획; 대상의 생명 상태 검사 |
| 35 | Kill | beast, human, predator | 확정 처치 가능한 상황만 허용; 기존 전투 결과 없이 즉사 처리 금지 |
| 36 | InspectBody | human | 같은 월드의 사체·도달성·조사 상태 |
| 37 | Loot | human | 사체 재고·소유권·실제 이전 |
| 38 | Mourn | human | 사망 인지·애도·관계 기억 |
| 39 | Report | human | 신고할 사건·인지 신뢰도·정착지 치안 시스템 |
