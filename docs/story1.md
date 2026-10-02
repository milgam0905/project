\[DOCUMENT]

id=DOC\_AERIA\_001

title=잔향의 땅 : ECHOES OF AERIA — 스토리 및 영속 세계 시스템 DB

version=1.0

language=ko-KR

encoding=UTF-8

artifact\_type=독립 설계 및 seed 데이터 TXT; 실행 가능한 DB나 게임 구현은 아님

scope=싱글플레이 쿼터뷰 2.5D 오픈월드 RPG; 프롤로그와 초반 1막

source=참조 대화의 영속 세계 및 WBG 개념과 사용자가 지정한 세계관 요소

proposal\_boundary=인물의 상세 배경, 사건 전개, 수치, 에덴의 진실은 연결성을 위해 작성한 제안이며 확정된 기존 프로젝트 설정으로 간주하지 않음



\[FORMAT]

id=FMT\_001

record\_start=\[TYPE] 한 줄로 레코드 시작; 동일 TYPE 반복 가능

fields=필드명은 소문자 snake\_case; 한 줄에 key=value; 첫 번째 등호만 구분자로 사용

value\_types=모든 값은 UTF-8 문자열; 접미사 \_json 필드는 표준 JSON으로 파싱; 명시된 숫자/불리언 필드는 스키마에 따라 변환

escaping=일반 문자열의 줄바꿈은 역슬래시 n으로 표기; 빈 값은 빈 문자열; null은 값 없음

primary\_key=각 레코드의 id; 전 문서에서 중복 금지

foreign\_keys=참조 대상은 \_id 또는 \_ids\_json; 설명 및 조건식 안의 ID도 같은 레코드를 지칭

expressions=조건·효과 설명은 선언형 설계 언어; 임의 코드를 eval하지 않고 허용된 연산자로 변환

conditions=AND/OR/NOT, 비교, alive(id), flag(name), inventory(id,item), state(id,key), resolved(id), outcome(id) 지원

conversion=TYPE별 테이블 또는 JSON 배열 생성; 효과 순서와 참조 무결성 검사 후 로드

units=world\_minute는 게임 시간 정수 분; 자원은 실제 수량; 상태 지수는 0\~100; 평판·관계는 -100\~100



\[WORLD]

id=WORLD\_AERIA

name=아에리아

calendar=대붕괴력 300년 봄 1일 06:00을 world\_minute=0으로 정의

premise=고대 아르카 문명은 에테르를 동력과 기억 저장 매체로 사용하여 도시와 생태계를 연결했다. 300년 전 세계탑의 동조 실패로 대붕괴가 발생했고, 생존자들은 단절된 지역에서 왕국과 자치 마을을 세웠다.

aether=생명과 기억에 반응하는 에너지; 적정 농도는 기술과 생태 순환을 돕지만 과잉 농도는 기억 혼선과 생물의 변이를 일으킴

protagonist=주인공은 레나 외곽의 폐쇄된 아르카 수로에서 기억 없이 깨어난다. 손목의 인장은 세계탑과 에덴 시설에 반응한다.

lia=리아는 레나의 유적 기록 조사자이며 주인공을 처음 발견한다. 아르카 기록을 해독할 수 있지만 에덴의 진실 전체는 모른다.

eden\_project=대붕괴 이후의 생태 복원과 기억 보존을 목표로 시작된 아르카 계획. 제안된 진실은 세계 복원을 위해 살아 있는 존재의 기억을 공통망에 동기화하는 통제 장치가 함께 설계되었다는 것.

world\_tower=에테르 흐름을 조정하는 중앙 시설. 마을의 이상현상은 분리된 하위 시설이 다시 동조하기 시작한 징후다.

central\_question=세계를 복구하는 대가로 개인의 기억과 선택권을 어디까지 넘길 것인가

long\_arc=레나의 생존 위기 조사 → 에덴 중계소 발견 → 세력별 복원 계획 비교 → 세계탑 도달 → 중앙 동조/분산 복원/봉쇄의 대가 선택

act1\_boundary=세계탑 입장까지 진행하지 않음; 초반 1막 종료 시 위치 좌표와 에덴 접근 열쇠 확보

player\_agency=플레이어 선택은 자동 선택하지 않으며 선택 가중치는 결과 전망과 NPC 반응의 근거로만 사용



\[REGION]

id=REG\_LENA\_VALLEY

name=레나 계곡

world\_id=WORLD\_AERIA

settlement\_ids\_json=\["SET\_LENA","SET\_ORUN"]

features=농경지, 사슴 숲, 늑대 능선, 아르카 수로, 옛 중계소

state\_json={"rainfall":55,"soil\_health":68,"aether\_pollution":22,"water\_quality":72,"vegetation":70,"carrying\_capacity\_deer":48,"carrying\_capacity\_wolf":10}

resource\_flows=물은 상류 수로에서 농경지로 흐름; 상단은 오룬에서 레나까지 이동 시간과 위험을 소비



\[REGION]

id=REG\_TOWER\_FRONTIER

name=세계탑 변경

world\_id=WORLD\_AERIA

state\_json={"aether\_pollution":70,"security":18,"water\_quality":30}

access=초반에는 좌표와 탐사 소문만 공개; 실제 진입은 후속 막에서 설계



\[SETTLEMENT]

id=SET\_LENA

name=레나

region\_id=REG\_LENA\_VALLEY

state\_json={"population":84,"food\_stock":252,"food\_target\_days":5,"water\_stock":168,"treasury":420,"security":58,"economy":52,"morale":60,"aether\_pollution":22,"player\_reputation":0,"housing\_capacity":96}

food\_unit=성인 1명이 하루 먹는 배급량 1단위; 초기 식량 3일분

food\_index=clamp(food\_stock / max(1,population\*food\_target\_days)\*100,0,100); 초기 60

faction\_influence\_json={"FAC\_KINGDOM":45,"FAC\_MERCHANT":35,"FAC\_RAVEN":20}

population\_accounting=명명 NPC 6명과 비명명 주민 78명; 플레이어·야생동물 제외; 리아와 노아도 주민 수에 포함

occupations\_json={"farm":28,"craft":12,"guard":6,"trade":8,"care":10,"other":20}

production=기준 농업 생산 100배급/일 × 계절 × 노동 가동률 × 수질 × 토양 보정; 운송 도착분은 별도 입고



\[SETTLEMENT]

id=SET\_ORUN

name=오룬 교역소

region\_id=REG\_LENA\_VALLEY

state\_json={"population":40,"food\_stock":320,"food\_target\_days":5,"water\_stock":140,"treasury":800,"security":65,"economy":68,"morale":63,"aether\_pollution":10,"player\_reputation":0,"housing\_capacity":60}

faction\_influence\_json={"FAC\_KINGDOM":25,"FAC\_MERCHANT":70,"FAC\_RAVEN":5}

population\_accounting=초기 비명명 주민 40명; 상단 인원은 주민 소속을 유지한 채 이동



\[FACTION]

id=FAC\_KINGDOM

name=왕국군

goal=질서 유지와 아르카 시설의 군사적 통제

policy=신고 증거에 따라 조사; 식량 지원에는 징발권과 주둔 조건 요구

resources\_json={"regional\_troops":36,"supply":180,"funds":1200}

relations\_json={"FAC\_MERCHANT":25,"FAC\_RAVEN":-65}



\[FACTION]

id=FAC\_MERCHANT

name=상인연합

goal=안전한 교역과 에테르 기술의 독점적 유통

policy=공급 위험과 운송비에 따라 가격 책정; 장기 외상은 영향력과 채권 증가

resources\_json={"caravans":3,"supply":400,"funds":2400}

relations\_json={"FAC\_KINGDOM":25,"FAC\_RAVEN":-20}



\[FACTION]

id=FAC\_RAVEN

name=회색 까마귀

goal=왕국의 유물 독점 저지와 자치권 확보

policy=은밀한 정보 거래와 창고 침입; 구성원마다 폭력 허용 정도가 다름

resources\_json={"operatives":12,"supply":70,"funds":360}

relations\_json={"FAC\_KINGDOM":-65,"FAC\_MERCHANT":-20}



\[SIMULATION]

id=SIM\_CLOCK

world\_id=WORLD\_AERIA

clock=게임 실행 중 일시정지하지 않은 시간만 흐름; 현실 20분=게임 1일인 속도를 기본 제안으로 사용

pause=메뉴 일시정지와 저장 로드는 세계 시간을 늘리지 않음; 게임 종료 중 실제 시간 경과는 기본적으로 반영하지 않음

time\_scale=현실 1초=world\_time 1.2분; 전투 반응 주기는 현실 초이며 경제 시간과 분리

schedules\_json={"abstract\_step\_world\_minutes":10,"settlement\_step\_world\_minutes":60,"ecology\_step\_world\_minutes":1440,"full\_perception\_real\_seconds":0.25,"full\_decision\_real\_seconds":1}

determinism=world\_seed + entity\_id + tick\_index + purpose로 난수 스트림 분리; 저장과 로드로 결과 재추첨 금지

event\_boundary=큰 시간 건너뛰기도 예약 사건의 시각에서 분할; 도착·배급·전투·사망의 시간 순서 보존



\[SIMULATION]

id=SIM\_PERSISTENCE

storage=world\_time, 개체 상태, 인벤토리, 예약 큐, 사건 상태, 난수 카운터, 증거, 기억, 관계, 자원 원장, LOD 상태 저장

settlement\_cycle=생산 예약 → 실제 수확 → 운송 도착 → 배급 → 부족량 산출 → 건강·사기·범죄 압력 → 세금·시설 → 이주 판단

food\_shortage=식량 부족 시 우선 비축 구매와 배급 축소; 하루 부족만으로 즉사하지 않음; 개체 영양 결핍 누적이 건강에 반영

economy=food\_price=base\_price \* clamp(1+scarcity\*1.5+route\_risk\*0.5,0.6,3.0); scarcity와 route\_risk는 0\~1

security=순찰 시간·실제 사건·주민 협조·병력 피로로 갱신; 범죄 압력과 범죄 발생은 서로 다른 상태

population=next=previous+출생+도착 이주-사망-출발 이주; 임신 기간·이동 시간·주거 여유를 확인; 빈자리에 즉시 주민 생성 금지

factions=지원·징발·채무·범죄 수사·정보 공개로 영향력 이동; 세력 영향력 합 100을 유지하며 수혜 외 세력에서 비례 차감

environment=강수·수질·토양·오염의 흐름을 계산; 오염 제거 장치는 처리량과 연료 한계를 가짐

cascade=오염 상승 → 생산 감소 → 배급 부족 → 가격 상승 → 절도 동기 증가 → 치안 저하 → 교역 지연 → 추가 부족

no\_player\_dependency=발생 조건은 지역 상태와 world\_time; 플레이어 근접 여부는 표현 상세도만 바꿈



\[SIMULATION]

id=SIM\_LOD

full\_mode=플레이어 120m 이내 또는 현재 직접 상호작용 중인 개체; 경로·시야·청각·피격·회피·엄폐를 개별 계산

abstract\_mode=플레이어 180m 밖에서 10 world\_minutes 동안 유지되고 상호작용이 끝난 개체; 직업·여행·갈등·자원 소비를 일정 단위로 계산

hysteresis=120\~180m에서는 기존 모드 유지하여 경계 진동 방지

same\_semantics=두 모드 모두 동일한 행동 가능 조건·살해 조건·소유권·관계·증거·사망 규칙 사용

abstract\_combat=힘·건강·수적 우세·도주 경로를 바탕으로 접촉/부상/도주/항복/사망을 순서대로 판정; 단일 살해 확률로 대체하지 않음

handoff=시간 경계까지 이전 모드 처리 → 행동 진행률·목표·자원 예약·위치·부상·난수 인계 → 새 모드 시작

materialization=원거리에서 이미 발생한 사망을 근거리 진입 시 되돌리지 않음; 이동은 경로상의 합법 위치에 복원

cohorts=비명명 주민은 직업별 코호트 가능; 범죄자·목격자·관계 당사자가 되는 순간 고유 NPC ID와 개체 상태로 승격

budgets=먼 지역은 지연 처리 가능하나 시간 부채를 기록하고 진입 전 정산; 사건 결과를 생략하거나 중복 적용하지 않음



\[ECOLOGY]

id=ECO\_LENA

region\_id=REG\_LENA\_VALLEY

initial\_json={"wolf\_population":7,"deer\_population":32,"plant\_biomass":700,"carcass\_food":0}

identity=초기 늑대 7마리와 사슴 32마리는 개체 ID를 할당; 종별 총량은 개체 집계이며 별도의 추가 개체가 아님

cycle=식물 성장 → 사슴 섭식·이동 → 늑대 사냥 → 사체 소비·분해 → 번식·질병·이주

predation=Hunt 성공은 즉시 먹이 획득이 아님; 추적·공격·사망 확인 후 사체 섭식으로 열량 이동

population\_rules=태어난 새끼만 증가; 사망·이동은 출발과 도착을 함께 기록; 먹이 부족 시 번식률 하락 및 다른 구역 이동

pollution=오염은 수질과 먹이 경로로 노출량에 누적; 높은 노출량은 질병·방향 감각 저하를 먼저 일으키고 무조건적 공격성 부여 금지

feedback=늑대 과잉 사냥 → 사슴 증가 → 식물 감소 → 농작물 피해; 사슴 급감 → 늑대 기아 → 가축 접근 위험 증가

carcass=사체의 무게와 남은 열량은 유한; Loot와 Eat에서 같은 자원을 중복 취득하지 못함



\[NPC\_SCHEMA]

id=SCHEMA\_NPC

identity\_json=\["id","name","species\_id","settlement\_id","faction\_id","alive","age\_years"]

state\_json={"health":"0\~100","stamina":"0\~100","position":"지역과 좌표","inventory":"item\_id:수량","current\_node\_id":"WBG 참조","target\_id":"nullable","action\_progress":"0\~1"}

personality\_json={"courage":"0\~100","empathy":"0\~100","aggression":"0\~100","lawfulness":"0\~100","greed":"0\~100","curiosity":"0\~100","loyalty":"0\~100"}

needs\_json={"hunger":"0\~100 높을수록 결핍","thirst":"0\~100","fatigue":"0\~100","safety":"0\~100 위험감","social":"0\~100 고립감"}

emotions\_json={"fear":"0\~100","anger":"0\~100","grief":"0\~100","hope":"0\~100"}

relationships=방향성 별도 레코드; 호감·신뢰·공포·원한·친족 여부 분리

memories=경험 시각·원인 사건·대상·확신·감정 강도·전파 출처·감쇠율·증거 참조

objectives=목표 목록과 우선도·기한·대상·진행도; 상황에 따라 우선도 조정

knowledge\_boundary=NPC는 목격·전달·조사로 얻은 정보만 판단에 사용; 전역 진실에 직접 접근하지 않음

seed\_defaults=alive=true; age\_years=30; health=100; stamina=80; inventory={}; personality 모든 축=50; needs 모든 축=20; emotions 모든 축=0; faction\_id=null



\[NPC]

id=NPC\_PLAYER

name=기억을 잃은 주인공

species\_id=SP\_HUMAN

settlement\_id=null

age\_years=25

role=플레이어 조작; 나이는 외견 기준 제안

inventory\_json={"ITEM\_EDEN\_MARK":1}

goals\_json=\[{"key":"recover\_memory","priority":80},{"key":"survive","priority":100}]

autonomy=수동 조작; 선택지를 WBG가 대신 선택하지 않음



\[NPC]

id=NPC\_LIA

name=리아

species\_id=SP\_HUMAN

settlement\_id=SET\_LENA

age\_years=27

role=유적 기록 조사자

personality\_json={"courage":62,"empathy":78,"aggression":15,"lawfulness":48,"greed":20,"curiosity":95,"loyalty":72}

goals\_json=\[{"key":"decode\_eden","priority":85},{"key":"protect\_lena","priority":70}]

secret=리아의 스승이 남긴 지도에는 레나 수로와 세계탑을 연결하는 동일한 문양이 있다

schedule=주간 수로 조사; 야간 진료소 기록 정리



\[NPC]

id=NPC\_EDWIN

name=에드윈

species\_id=SP\_HUMAN

settlement\_id=SET\_LENA

age\_years=48

role=레나 촌장 겸 곡물 관리자

personality\_json={"courage":55,"empathy":72,"aggression":20,"lawfulness":70,"greed":25,"curiosity":40,"loyalty":90}

needs\_json={"hunger":25,"thirst":20,"fatigue":45,"safety":35,"social":20}

emotions\_json={"fear":35,"anger":15,"grief":10,"hope":55}

goals\_json=\[{"key":"secure\_food","priority":90},{"key":"preserve\_autonomy","priority":65}]

inventory\_json={"ITEM\_COIN":45}

schedule=06:00 창고 점검; 09:00 민원; 18:00 배급; 야간 귀가



\[NPC]

id=NPC\_MARA

name=마라

species\_id=SP\_HUMAN

settlement\_id=SET\_LENA

faction\_id=FAC\_MERCHANT

age\_years=34

role=상단 중개인

personality\_json={"courage":48,"empathy":45,"aggression":18,"lawfulness":60,"greed":78,"curiosity":50,"loyalty":55}

goals\_json=\[{"key":"keep\_trade\_route","priority":85},{"key":"collect\_debt","priority":60}]

inventory\_json={"ITEM\_COIN":120}



\[NPC]

id=NPC\_DARIAN

name=다리안

species\_id=SP\_HUMAN

settlement\_id=SET\_LENA

faction\_id=FAC\_KINGDOM

age\_years=39

role=주둔 분대장

personality\_json={"courage":82,"empathy":40,"aggression":45,"lawfulness":88,"greed":15,"curiosity":35,"loyalty":80}

goals\_json=\[{"key":"keep\_order","priority":90},{"key":"secure\_relay","priority":72}]

kill\_policy=항복자는 구금 우선; 공포나 명령만으로 Kill 허용 안 됨



\[NPC]

id=NPC\_NOAH

name=노아

species\_id=SP\_HUMAN

settlement\_id=SET\_LENA

faction\_id=FAC\_RAVEN

age\_years=29

role=약초꾼으로 활동하는 회색 까마귀 연락책

personality\_json={"courage":70,"empathy":64,"aggression":35,"lawfulness":25,"greed":30,"curiosity":72,"loyalty":85}

goals\_json=\[{"key":"expose\_eden","priority":80},{"key":"avoid\_arrest","priority":75}]



\[NPC]

id=NPC\_SELIN

name=셀린

species\_id=SP\_HUMAN

settlement\_id=SET\_LENA

age\_years=42

role=치료사; 리아가 사망하거나 부재하면 기록 보관 책임을 승계

personality\_json={"courage":58,"empathy":95,"aggression":5,"lawfulness":68,"greed":10,"curiosity":65,"loyalty":75}

goals\_json=\[{"key":"treat\_exposure","priority":95},{"key":"protect\_patients","priority":85}]



\[RELATIONSHIP]

id=REL\_EDWIN\_LIA

from\_id=NPC\_EDWIN

to\_id=NPC\_LIA

values\_json={"affinity":55,"trust":65,"fear":0,"grudge":0,"kinship":false}



\[RELATIONSHIP]

id=REL\_LIA\_EDWIN

from\_id=NPC\_LIA

to\_id=NPC\_EDWIN

values\_json={"affinity":60,"trust":50,"fear":0,"grudge":0,"kinship":false}



\[RELATIONSHIP]

id=REL\_DARIAN\_NOAH

from\_id=NPC\_DARIAN

to\_id=NPC\_NOAH

values\_json={"affinity":-15,"trust":-25,"fear":0,"grudge":5,"kinship":false}

knowledge=의심만 존재; 회색 까마귀 신분을 확정적으로 알지 못함



\[MEMORY]

id=MEM\_EDWIN\_FAMINE

owner\_id=NPC\_EDWIN

subject\_id=SET\_LENA

kind=과거 흉년

occurred\_world\_minute=-525600

confidence=1.0

intensity=75

decay\_per\_day=0.001

source=직접 경험

behavior\_effect=식량 확보 GoalWeight +15; 비축 없는 상태에서 Trade·Help 가중치 상승



\[MEMORY]

id=MEM\_LIA\_RESCUE

owner\_id=NPC\_LIA

subject\_id=NPC\_PLAYER

kind=수로에서 발견한 생존자

created\_on\_event\_id=EVT\_P001

confidence=1.0

intensity=50

decay\_per\_day=0.01

behavior\_effect=Help +10; Protect +8; 인장이 활성화되면 Investigate +20



\[WBG\_RULE]

id=WBG\_RULE\_CORE

node\_count=39

architecture=Evaluate → Observe → Remember → Decide → 선택 행동 → 결과 기록 → Evaluate; Investigate는 지속 시간이 있는 선택 행동

control\_nodes\_json=\["WBG\_01","WBG\_02","WBG\_04","WBG\_05"]

control\_note=인지 노드는 매 평가 단계의 연산이며 Idle 등과 점수 경쟁하지 않음; 총 39개 수에는 포함

formula=FinalWeight = BaseWeight + NeedWeight + EmotionWeight + RelationshipWeight + EnvironmentWeight + PersonalityWeight + MemoryWeight + GoalWeight + RandomVariation

weights=BaseWeight 0\~100; 다른 항은 각각 -100\~100; RandomVariation -3\~3; 모든 항 계산 후 점수 -800\~803 허용

eligibility=종 활성 노드·생존·의식·자원·거리·타깃·특수 조건을 먼저 검사; 실행 불가는 음의 무한대이며 0점으로 취급하지 않음

selection=유효 후보 중 최대 점수; 동점은 기존 행동 우선, 다음은 노드 ID 순; 난수는 선택 시점마다 한 번만 생성

hysteresis=현재 행동보다 12점 이상 높고 최소 유지 시간이 지났을 때 전환; 즉각적 위험·타깃 소실·실행 불가는 즉시 중단 가능

graph=노드의 next\_ids\_json은 후보를 여는 간선; 실제 다음 행동은 매번 eligibility와 점수 검사를 거침; 모든 행동은 WBG\_01로 재평가 가능

traits=정규화한 욕구·성격·감정과 행동별 계수의 내적; 개별 항은 clamp(-100,100); 종과 직업은 계수 프로필로 차별화

interrupt=죽음은 흡수 상태이며 노드가 아님; 의식 상실은 상태 플래그; 별도 40번째 행동 노드를 추가하지 않음



\[WBG\_RULE]

id=WBG\_RULE\_KILL

attack=Attack은 위협을 제압하거나 먹이를 약화시키는 공격. 비살상 의도여도 실제 피해가 치명적이면 사망할 수 있음.

kill=Kill은 이미 제압되었거나 무력화된 대상의 생명을 의도적으로 끝내려는 별도 행동. 선택 시에도 사망 확정이 아닌 실행 판정 필요.

shared\_gate=alive(actor) AND alive(target) AND target.incapacitated AND reachable(target) AND actor.lethal\_capability AND actor.intent\_confirmed

human\_gate=shared\_gate AND (명시적 살해 목표 우선도>=90 또는 확신>=0.9인 중대한 직접 위협 지속) AND hesitation<20 AND alternatives\_evaluated=true

human\_inhibitors=항복 수용·보호 대상·정보 불확실·인질 구출 가능·오인 가능성은 Kill 차단 또는 망설임 상승; 원한 기억 하나만으로 gate 통과 불가

wolf\_gate=shared\_gate AND target가 사냥 먹이 AND hunger>=70 AND hunt\_commitment>=85; 단순 영역 침입은 해당하지 않음

deer\_gate=항상 false

recheck=시작과 실행 직전에 조건 재확인; 타깃이 이미 죽었으면 취소; 사망 처리는 공통 death transaction 1회

world\_consequence=고의 Kill과 Attack 도중 치명상은 의도와 증거를 구분하여 범죄 조사 및 기억에 기록



\[WBG\_EXAMPLE]

id=EX\_WEIGHT\_001

context=배고픈 주민; 멀리 적대자가 있으나 당장 공격받지 않음

term\_order\_json=\["BaseWeight","NeedWeight","EmotionWeight","RelationshipWeight","EnvironmentWeight","PersonalityWeight","MemoryWeight","GoalWeight","RandomVariation"]

eat\_json=\[10,55,0,0,5,0,0,5,0]

eat\_total=75

flee\_json=\[10,20,10,0,2,0,0,0,0]

flee\_total=42

attack\_json=\[10,5,10,-4,-5,5,5,5,0]

attack\_total=31

result=Eat 선택; 안전한 취침 장소가 없는 Sleep은 후보 제외



\[WBG\_EXAMPLE]

id=EX\_WEIGHT\_002

context=같은 주민에게 무장 상대가 공격; 즉각 위험으로 기존 Eat 중단

term\_order\_json=\["BaseWeight","NeedWeight","EmotionWeight","RelationshipWeight","EnvironmentWeight","PersonalityWeight","MemoryWeight","GoalWeight","RandomVariation"]

flee\_json=\[10,30,25,0,15,5,0,6,0]

flee\_total=91

attack\_json=\[10,10,20,5,10,5,5,7,0]

attack\_total=72

callhelp\_json=\[10,15,15,5,10,5,0,5,0]

callhelp\_total=65

eat\_total=25

result=Flee 선택; Kill은 무력화된 대상도 없고 살해 의지도 없어 점수 계산 이전에 제외



\[WBG\_NODE]

id=WBG\_01

name=Evaluate

label=상태 평가

preconditions=생존 개체

result=상태 스냅샷과 긴급 위험 판정

base\_weight=10

next\_ids\_json=\["WBG\_02"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_02

name=Observe

label=주변 관찰

preconditions=감각 사용 가능

result=시야·소리·냄새로 관측된 정보만 수집

base\_weight=10

next\_ids\_json=\["WBG\_04","WBG\_03","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_03

name=Investigate

label=이상 현상 조사

preconditions=조사할 단서와 접근 경로 존재

result=단서 확신 상승; 이동 시간과 위험 소비

base\_weight=10

next\_ids\_json=\["WBG\_02","WBG\_38","WBG\_39","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_04

name=Remember

label=기억 조회

preconditions=인지 처리 가능

result=현재 대상과 관련된 기억만 활성화

base\_weight=10

next\_ids\_json=\["WBG\_05","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_05

name=Decide

label=행동 결정

preconditions=유효한 상태 스냅샷

result=가능 조건 필터 후 효용 최대 후보 선택

base\_weight=10

next\_ids\_json=\["WBG\_06","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_06

name=Idle

label=대기

preconditions=생존 및 의식 있음

result=짧은 대기; 욕구와 시간 계속 변화

base\_weight=10

next\_ids\_json=\["WBG\_02","WBG\_07","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_07

name=Wander

label=배회

preconditions=이동 가능

result=종별 생활권 내부의 탐색 목적 이동

base\_weight=10

next\_ids\_json=\["WBG\_03","WBG\_08","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_08

name=Travel

label=목적지 이동

preconditions=목적지와 경로 존재

result=경로 진행률·체력·도착 시각 기록

base\_weight=10

next\_ids\_json=\["WBG\_12","WBG\_10","WBG\_11","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_09

name=Sleep

label=수면

preconditions=안전한 장소; 긴급 위험 없음

result=피로 회복; 감각 저하

base\_weight=10

next\_ids\_json=\["WBG\_13","WBG\_02","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_10

name=Eat

label=섭식

preconditions=먹을 수 있는 식량; 즉각 위험 없음

result=소유·접근 가능한 자원 소비; 기아 감소

base\_weight=10

next\_ids\_json=\["WBG\_13","WBG\_11","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_11

name=Drink

label=음수

preconditions=접근 가능한 수원

result=갈증 감소; 오염된 물은 노출 증가

base\_weight=10

next\_ids\_json=\["WBG\_13","WBG\_08","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_12

name=Work

label=노동

preconditions=직업 도구·작업장·업무 존재

result=노동 시간 누적 후 생산 예약; 즉시 식량 생성 금지

base\_weight=10

next\_ids\_json=\["WBG\_13","WBG\_18","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_13

name=Rest

label=휴식

preconditions=치명적 위험 없음

result=체력 회복; 수면보다 낮은 피로 회복

base\_weight=10

next\_ids\_json=\["WBG\_02","WBG\_09","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_14

name=Talk

label=대화

preconditions=언어를 공유하는 상대

result=정보 전달과 관계 변화; 지식 출처 기록

base\_weight=10

next\_ids\_json=\["WBG\_17","WBG\_22","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_15

name=Greet

label=인사

preconditions=인사 가능한 사회적 상대

result=호감 소폭 변화; 쿨다운 적용

base\_weight=10

next\_ids\_json=\["WBG\_14","WBG\_18","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_16

name=Trade

label=거래

preconditions=쌍방 동의·재고·대금 확인

result=상품·화폐·소유권을 원자적으로 교환

base\_weight=10

next\_ids\_json=\["WBG\_08","WBG\_14","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_17

name=Help

label=돕기

preconditions=도움 필요한 상대; 제공 자원 또는 능력

result=치료·운반 등 파라미터로 도움 수행

base\_weight=10

next\_ids\_json=\["WBG\_33","WBG\_13","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_18

name=Follow

label=따라가기

preconditions=유효한 추종 대상

result=거리 유지; 인간 호위·동물 무리 이동

base\_weight=10

next\_ids\_json=\["WBG\_08","WBG\_33","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_19

name=Avoid

label=회피 이동

preconditions=기피 대상 및 우회 경로

result=접촉 위험 낮추기

base\_weight=10

next\_ids\_json=\["WBG\_08","WBG\_06","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_20

name=Warn

label=경고

preconditions=전달 가능한 위험 정보와 수신자

result=인간 말·동물 경계 신호; 진실 확신 보존

base\_weight=10

next\_ids\_json=\["WBG\_21","WBG\_31","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_21

name=CallHelp

label=지원 요청

preconditions=수신 가능한 동료

result=인간 구조 요청·늑대 울음; 청취자만 반응

base\_weight=10

next\_ids\_json=\["WBG\_29","WBG\_31","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_22

name=Argue

label=논쟁

preconditions=언어적 갈등과 대화 상대

result=감정·관계 변화; 자동 폭력 전환 아님

base\_weight=10

next\_ids\_json=\["WBG\_14","WBG\_23","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_23

name=Threaten

label=위협

preconditions=상대 인지 및 위협 능력

result=요구와 불응 시 행동 의사 전달

base\_weight=10

next\_ids\_json=\["WBG\_24","WBG\_27","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_24

name=Intimidate

label=위압

preconditions=상대와 감각 접촉

result=자세·소리로 공포 유발; 동물 과시 포함

base\_weight=10

next\_ids\_json=\["WBG\_19","WBG\_28","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_25

name=Steal

label=절도

preconditions=타인 소유 물품과 접근 기회

result=소유권 이전 시 범죄 사실 생성; 발각 별도 판정

base\_weight=10

next\_ids\_json=\["WBG\_31","WBG\_19","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_26

name=Chase

label=추격

preconditions=발견한 목표와 추적 가능 경로

result=거리 축소·체력 소비; 추적 상실 가능

base\_weight=10

next\_ids\_json=\["WBG\_28","WBG\_19","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_27

name=Surrender

label=항복

preconditions=상대에게 항복 전달 가능

result=전투 의도 해제; 상대 수용 여부 별도

base\_weight=10

next\_ids\_json=\["WBG\_18","WBG\_06","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_28

name=Attack

label=공격

preconditions=유효 적대·사냥 대상; 공격 수단·사거리

result=명중·방어·피해 판정; 우발적 사망 가능

base\_weight=10

next\_ids\_json=\["WBG\_29","WBG\_30","WBG\_31","WBG\_35","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_29

name=Defend

label=방어

preconditions=예상되는 공격과 방어 수단

result=피해 경감; 체력 소비

base\_weight=10

next\_ids\_json=\["WBG\_28","WBG\_31","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_30

name=Dodge

label=회피 동작

preconditions=공격 감지·공간·체력

result=회피 성공 판정; 이동·체력 소비

base\_weight=10

next\_ids\_json=\["WBG\_28","WBG\_31","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_31

name=Flee

label=도주

preconditions=위협 인지; 이동 가능

result=안전 지점 선택; 막히면 재평가

base\_weight=10

next\_ids\_json=\["WBG\_32","WBG\_21","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_32

name=SeekCover

label=엄폐

preconditions=사용 가능한 지형

result=시야·사선 차단; 동물은 수풀 은신

base\_weight=10

next\_ids\_json=\["WBG\_19","WBG\_13","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_33

name=Protect

label=보호

preconditions=보호 대상과 보호 능력

result=위협 사이에 개입하거나 안전 지점으로 호위

base\_weight=10

next\_ids\_json=\["WBG\_29","WBG\_28","WBG\_21","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_34

name=Hunt

label=사냥

preconditions=먹이 대상·사냥 능력

result=탐색과 매복·추적 계획; 사망 자동 확정 금지

base\_weight=10

next\_ids\_json=\["WBG\_26","WBG\_28","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_35

name=Kill

label=의도적 살해

preconditions=WBG\_RULE\_KILL의 종별 높은 gate 충족

result=실행 시 재검증; 성공한 치명 피해는 공통 사망 처리

base\_weight=10

next\_ids\_json=\["WBG\_36","WBG\_38","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_36

name=InspectBody

label=사체 조사

preconditions=실제 사체 접근 가능

result=신원·사인·증거 관찰; 동물은 냄새·먹이 판별

base\_weight=10

next\_ids\_json=\["WBG\_37","WBG\_39","WBG\_38","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_37

name=Loot

label=전리품 수거

preconditions=사체 또는 포기된 물품 접근

result=인간의 수거; 소유권과 범죄 여부 판정

base\_weight=10

next\_ids\_json=\["WBG\_08","WBG\_39","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_38

name=Mourn

label=애도

preconditions=상실을 인지한 유대 대상

result=슬픔 표출·관계 지지; 늑대는 무리 상실 반응

base\_weight=10

next\_ids\_json=\["WBG\_14","WBG\_13","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[WBG\_NODE]

id=WBG\_39

name=Report

label=신고·보고

preconditions=보고할 정보와 접수 상대

result=증거·확신·출처를 전달; 수사 큐 등록

base\_weight=10

next\_ids\_json=\["WBG\_14","WBG\_08","WBG\_01"]

termination=성공·실패·중단 시 진행률과 결과를 기록하고 재평가



\[SPECIES]

id=SP\_HUMAN

name=인간

active\_nodes=39개 전부 종 차원에서 허용; 실제 NPC의 직업·능력·성격과 실행 조건으로 제한

disabled\_nodes\_json=\[]

profile=언어·재산·법·직업·장기 목표 사용; Kill은 일반 활성 목록에 있어도 별도 gate 미충족이면 실행 불가



\[SPECIES]

id=SP\_WOLF

name=늑대

active\_nodes\_json=\["Evaluate","Observe","Investigate","Remember","Decide","Idle","Wander","Travel","Sleep","Eat","Drink","Rest","Help","Follow","Avoid","Warn","CallHelp","Intimidate","Chase","Attack","Defend","Dodge","Flee","SeekCover","Protect","Hunt","Kill","InspectBody","Mourn"]

disabled\_nodes\_json=\["Work","Talk","Greet","Trade","Argue","Threaten","Steal","Surrender","Loot","Report"]

profile=Help는 새끼 돌봄; Warn은 경계음; Mourn은 무리 상실 반응; 인간 법률·언어 추론을 사용하지 않음

needs=육식·무리 안전·번식 계절·영역을 목표 계수로 표현



\[SPECIES]

id=SP\_DEER

name=사슴

active\_nodes\_json=\["Evaluate","Observe","Investigate","Remember","Decide","Idle","Wander","Travel","Sleep","Eat","Drink","Rest","Follow","Avoid","Warn","Intimidate","Attack","Defend","Dodge","Flee","SeekCover","Protect","InspectBody"]

disabled\_nodes\_json=\["Work","Talk","Greet","Trade","Help","CallHelp","Argue","Threaten","Steal","Chase","Surrender","Hunt","Kill","Loot","Mourn","Report"]

profile=Attack은 퇴로가 막힌 방어·발굽 공격; Kill 비활성이어도 Attack의 치명상은 발생 가능; Protect는 새끼 보호

needs=초식·물·무리·계절 이동을 중심으로 점수 계산



\[STORY\_RULE]

id=STORY\_RULE\_001

quest\_states\_json=\["dormant","eligible","active","resolved","expired"]

event\_instance=event\_id + occurrence\_index; once=true 사건은 1회만 인스턴스 생성

trigger\_policy=이벤트가 활성화된 절대 시각을 저장; 플레이어가 퀘스트를 수락하지 않아도 deadline은 진행

choice\_policy=한 사건 인스턴스당 하나의 해결 선택만 원자적으로 확정; 조사 대화는 해결 선택과 별개

race\_policy=플레이어 입력 시각과 자동처리 deadline 중 먼저 도달한 유효 요청 승리; 같은 시각은 자동처리 우선

fallback=주요 NPC 사망·이주·적대화로 대화가 막히면 기록·소지품·현장 단서로 주 줄거리 계속; 개인 구출과 관계 보상은 되돌리지 않음

resolution=선택 또는 미개입이 outcome을 생성하고 효과를 적용한 다음 후속 이벤트 조건 재평가



\[EVENT]

id=EVT\_P001

title=물 아래에서 들린 이름

phase=프롤로그

once=true

trigger=world\_minute=0

conditions=새 게임

story=수로 바닥에서 깨어난 주인공은 끊어진 도시와 거대한 탑의 잔상을 본다. 리아는 손목 인장을 발견하고 레나로 안내한다.

choice\_ids\_json=\["CHO\_P001\_TRUST","CHO\_P001\_ALONE"]

deadline\_minutes=30

no\_intervention=리아가 구조를 시도하고 기록을 남김; 주인공은 수로 출구를 직접 따라갈 수 있음; 인장은 유지

outcome=rescued 또는 self\_exit; 어느 경우에도 첫 기억 단서 보존

next\_event\_ids\_json=\["EVT\_P002"]



\[EVENT]

id=EVT\_P002

title=레나의 빈 그릇

phase=프롤로그

once=true

trigger=resolved(EVT\_P001)

conditions=레나 또는 원거리 소식 경로 사용 가능

story=에드윈은 수로 오염과 곡물 감소를 알린다. 주민은 늑대를 원인으로 의심하지만 리아는 물속의 금속성 빛을 지적한다.

choice\_ids\_json=\["CHO\_P002\_WATER","CHO\_P002\_FOREST"]

deadline\_minutes=120

no\_intervention=에드윈이 주민 조사대를 편성; 노출과 노동 공백 발생; 조사 결과를 게시판에 보존

outcome=수질 우선/숲 우선/주민 조사; 모든 경로에서 두 원인의 존재를 확인 가능

next\_event\_ids\_json=\["EVT\_A101","EVT\_A102"]



\[EVENT]

id=EVT\_A101

title=배급과 계약

phase=초반 1막

once=true

trigger=world\_minute>=360 AND state(SET\_LENA,food\_index)<60

conditions=state(SET\_LENA,population)>0

story=곡물 저장고의 부족이 드러나며 마라가 긴급 구매 계약을 제안한다.

choice\_ids\_json=\["CHO\_A101\_BUY","CHO\_A101\_STEAL","CHO\_A101\_RATION"]

deadline\_minutes=720

no\_intervention=창고 책임자가 예산 120 이상이면 오룬에 60배급 구매 주문; 아니면 하루 0.75배급 체제로 전환; 영양 결핍 누적

outcome=구매/탈취/배급/자동조달 중 하나; 운송 완료 전 식량은 증가하지 않음

next\_event\_ids\_json=\["EVT\_A103"]



\[EVENT]

id=EVT\_A102

title=숲의 굶주린 발자국

phase=초반 1막

once=true

trigger=world\_minute>=180

conditions=ECO\_LENA의 늑대 개체가 1마리 이상 생존

story=오염으로 이동한 사슴을 쫓던 늑대가 농경지와 가까워진다. 늑대 전멸은 단기 안전과 장기 생태 피해를 함께 낳을 수 있다.

choice\_ids\_json=\["CHO\_A102\_RESTORE","CHO\_A102\_HUNT"]

deadline\_minutes=720

no\_intervention=주민 사냥대가 실제 인원·체력으로 출동; 동물과 동일 전투 규칙 적용; 사망 수를 미리 고정하지 않음

outcome=서식지 회복/사냥/자율 충돌; 늑대가 이미 멸종하면 expired로 기록

next\_event\_ids\_json=\["EVT\_A104"]



\[EVENT]

id=EVT\_A103

title=멈춘 수레

phase=초반 1막

once=true

trigger=world\_minute>=1080

conditions=레나-오룬 도로 이용 가능 또는 우회 조사 가능

story=에테르 균열로 교역 수레가 멈췄다. 회색 까마귀의 표식이 남았지만 이것만으로 습격을 확정할 수 없다.

choice\_ids\_json=\["CHO\_A103\_ESCORT","CHO\_A103\_DEAL"]

deadline\_minutes=480

no\_intervention=상단이 위험과 계약 이익을 평가해 우회하거나 철수; 실제 예약 화물만 지연·반송

outcome=호위/정보 거래/우회/철수; 중계소 위치 단서는 현장에도 남음

next\_event\_ids\_json=\["EVT\_A104","EVT\_A105"]



\[EVENT]

id=EVT\_A104

title=아르카 수로의 문

phase=초반 1막

once=true

trigger=world\_minute>=720

conditions=인장 또는 리아의 지도 사본이 존재; 사본은 수로 벽면에서도 재구성 가능

story=수로 내부의 장치가 주인공을 복원 대상 07로 인식한다. 에덴이라는 이름이 처음 완전하게 재생된다.

choice\_ids\_json=\["CHO\_A104\_PURIFY","CHO\_A104\_SEAL"]

deadline\_minutes=1440

no\_intervention=시설이 과부하 보호 정지; 10 world\_minutes마다 오염 +1인 누출은 정지 후 중단; 누적 오염은 유지

outcome=정화/봉쇄/보호정지; 진입이 늦어도 비휘발성 기록과 중계소 좌표 회수 가능

next\_event\_ids\_json=\["EVT\_A106"]



\[EVENT]

id=EVT\_A105

title=누가 곡물을 가져갔는가

phase=초반 1막의 조건부 사건

once=true

trigger=레나 식량 관련 범죄가 신고되거나 순찰이 증거 발견

conditions=수사 가능한 담당자 또는 승계자 존재

story=다리안은 창고 절도와 상단 사고의 연관성을 조사한다. 굶주린 주민의 소문은 물증과 구분된다.

choice\_ids\_json=\["CHO\_A105\_EVIDENCE","CHO\_A105\_HIDE"]

deadline\_minutes=720

no\_intervention=조사관이 증거 확신으로 수사 지속 또는 보류; 플레이어를 자동 범인 지정하지 않음

outcome=증거 제출/은닉 시도/독립 수사; 오인 처분 시 불신과 항의 사건 씨앗 생성

next\_event\_ids\_json=\["EVT\_A106"]



\[EVENT]

id=EVT\_A106

title=세 갈래의 제안

phase=초반 1막

once=true

trigger=world\_minute>=1800

conditions=수로 기록 또는 공개된 에덴 소문 존재

story=왕국군은 시설 봉쇄를, 상인연합은 가동권 계약을, 회색 까마귀는 기록 공개를 요구한다.

choice\_ids\_json=\["CHO\_A106\_KINGDOM","CHO\_A106\_MERCHANT","CHO\_A106\_RAVEN","CHO\_A106\_INDEPENDENT"]

deadline\_minutes=1440

no\_intervention=레나의 영향력 최상위 세력이 임시 관리권 확보; 동률이면 촌장 중립 보관; 이미 사망한 대표는 조직의 후임이 수행

outcome=관리권 세력 확정 또는 주민 공동 관리; 주인공의 열쇠 자체를 원격 강탈하지 않음

next\_event\_ids\_json=\["EVT\_A107"]



\[EVENT]

id=EVT\_A107

title=세계탑의 응답

phase=초반 1막 마감

once=true

trigger=world\_minute>=3240

conditions=세계탑 중계 신호의 예약 시각 도달

story=중계소는 300년 만에 세계탑 좌표를 송신한다. 에덴 기록은 세계 복구와 기억 동조가 하나의 계획임을 암시한다.

choice\_ids\_json=\["CHO\_A107\_DEPART","CHO\_A107\_STAY"]

deadline\_minutes=360

no\_intervention=신호가 기록 장치에 저장되고 탐사 가능 상태 지속; 마을 사건은 계속 진행

outcome=떠남/잔류/미청취; 신호를 나중에 읽어도 후속 막 접근 가능

next\_event\_ids\_json=\[]



\[EVENT]

id=EVT\_SYS\_DEATH

once=false

title=사망과 남겨진 사람들

trigger=개체의 health<=0 또는 회복 불가능 상태 판정

conditions=death\_processed=false

choice\_ids\_json=\[]

deadline\_minutes=0

no\_intervention=EFF\_DEATH\_COMMIT 실행; 시체 발견과 죽음을 아는 것은 분리

next\_event\_ids\_json=\["EVT\_SYS\_REPORT","EVT\_SYS\_GRIEF"]



\[EVENT]

id=EVT\_SYS\_REPORT

once=false

title=목격과 신고

trigger=범죄·사체·위험에 대한 인지 정보 획득

conditions=해당 정보의 목격자 또는 전달자가 존재

choice\_ids\_json=\[]

deadline\_minutes=0

no\_intervention=목격자 WBG에서 Flee/Help/Report 등을 경쟁 평가; 신고가 선택되고 접수자에게 도착한 경우만 수사 시작

next\_event\_ids\_json=\["EVT\_A105","EVT\_SYS\_GRIEF"]



\[EVENT]

id=EVT\_SYS\_GRIEF

once=false

title=상실의 기억

trigger=관계 대상의 사망 소식을 새로 인지

conditions=동일 사망에 대한 기억 미생성

choice\_ids\_json=\[]

deadline\_minutes=0

no\_intervention=슬픔과 유대 강도에 따라 Mourn/Help/Protect/회피/조사 목표 변화; 책임자 확신이 높으면 복수 목표가 후보가 되지만 살해 확정 아님

next\_event\_ids\_json=\[]



\[CHOICE\_RULE]

id=CHO\_RULE\_001

scoring=OptionWeight = SurvivalBenefit + ValueFit + RelationshipImpact + LongTermGoal - ResourceCost - RiskCost; 항목별 -100\~100

usage=NPC의 자동 정책 선택에 사용; 플레이어에게는 이익·대가·불확실성 설명으로 제공; 플레이어 입력을 대체하지 않음

example\_json={"buy":\[70,40,20,20,45,10],"steal":\[80,-30,10,5,15,70],"ration":\[35,20,0,15,5,15]}

example\_totals\_json={"buy":95,"steal":-20,"ration":50}

context=합법성과 주민 보호를 중시하는 에드윈 기준; 자금 부족이면 buy는 낮은 점수가 아니라 실행 불가; 과거 흉년 기억이 생존 이익 평가를 높임

uncertainty=예상 결과와 확정 효과 구분; 성공 확률은 이용 가능한 지식 기준이며 숨은 진실 표시 금지



\[CHOICE]

id=CHO\_P001\_TRUST

event\_id=EVT\_P001

label=리아를 믿고 동행

conditions=리아가 생존하고 대화 가능

effect\_ids\_json=\["EFF\_P001\_TRUST"]

outcome=rescued

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_P001\_TRUST

operations=관계 NPC\_PLAYER→NPC\_LIA trust +10; 안전한 출구로 이동 목표 설정; MEM\_LIA\_RESCUE 생성

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_P001\_ALONE

event\_id=EVT\_P001

label=혼자 출구를 찾기

conditions=이동 가능

effect\_ids\_json=\["EFF\_P001\_ALONE"]

outcome=self\_exit

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_P001\_ALONE

operations=수로 조사 단서 획득; 리아의 구조 기록을 보관; 신뢰 변화 없음

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_P002\_WATER

event\_id=EVT\_P002

label=수로 조사 우선

conditions=수로 접근 가능

effect\_ids\_json=\["EFF\_P002\_WATER"]

outcome=water

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_P002\_WATER

operations=수질 단서 지식 획득; EVT\_A104 접근 정보 공개; 식량을 즉시 늘리지 않음

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_P002\_FOREST

event\_id=EVT\_P002

label=숲의 흔적 조사 우선

conditions=숲 접근 가능

effect\_ids\_json=\["EFF\_P002\_FOREST"]

outcome=forest

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_P002\_FOREST

operations=사슴 이동과 늑대 기아 단서 지식 획득; EVT\_A102 대응 정보 공개

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_A101\_BUY

event\_id=EVT\_A101

label=60배급 구매를 지원

conditions=플레이어 코인>=120; 오룬 가용 재고>=60

effect\_ids\_json=\["EFF\_A101\_BUY"]

outcome=purchase

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_A101\_BUY

operations=플레이어 코인 120을 판매자에게 이전; 오룬 60배급을 화물로 예약; 경로 최소 240분 운송 후 레나 입고; 성공 인도 시 사기 +8·평판 +12

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_A101\_STEAL

event\_id=EVT\_A101

label=징발 창고에서 식량 탈취 시도

conditions=창고에 실제 접근; 소유 식량 존재

effect\_ids\_json=\["EFF\_A101\_STEAL"]

outcome=theft\_attempt

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_A101\_STEAL

operations=Steal 실행 과제 시작; 발각·실패 가능; 성공량만 창고→플레이어로 이전; 기부 시 플레이어→레나 이전; 범죄와 목격 판정 생성

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_A101\_RATION

event\_id=EVT\_A101

label=촌장과 임시 배급 합의

conditions=촌장 또는 승계자와 합의 가능

effect\_ids\_json=\["EFF\_A101\_RATION"]

outcome=ration

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_A101\_RATION

operations=1440분 동안 하루 0.75배급; 사기 -5; 남긴 식량은 소비 감소로만 계산; 결핍 누적

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_A102\_RESTORE

event\_id=EVT\_A102

label=먹이 이동 통로 복구

conditions=노동 120분 확보; 현장 위험 해소

effect\_ids\_json=\["EFF\_A102\_RESTORE"]

outcome=restore

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_A102\_RESTORE

operations=120분 작업 완료 시 오염 없는 이동 통로 개방; 사슴의 다음 이동 목표 갱신; 늑대 기아를 즉시 제거하지 않음

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_A102\_HUNT

event\_id=EVT\_A102

label=위협 늑대 사냥

conditions=추적 가능; 사냥 수단 보유

effect\_ids\_json=\["EFF\_A102\_HUNT"]

outcome=hunt

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_A102\_HUNT

operations=Hunt 목표 추가; 실제 전투 결과만 개체 수에 반영; 늑대 감소에 따른 사슴 개체수 변화는 일일 생태 갱신에서 처리

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_A103\_ESCORT

event\_id=EVT\_A103

label=상단 호위

conditions=상단이 아직 철수하지 않음

effect\_ids\_json=\["EFF\_A103\_ESCORT"]

outcome=escort

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_A103\_ESCORT

operations=Follow·Protect 호위 계약; 도착한 화물만 입고; 성공 시 실제 상단 자금에서 보수 25 이전 및 평판 +8

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_A103\_DEAL

event\_id=EVT\_A103

label=노아의 정보 제안을 수락

conditions=노아 또는 연락 기록 접근 가능

effect\_ids\_json=\["EFF\_A103\_DEAL"]

outcome=information

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_A103\_DEAL

operations=중계소 위치와 습격 의혹의 반증 단서 획득; 노아 trust +10; 거래 자체는 범죄로 확정하지 않음

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_A104\_PURIFY

event\_id=EVT\_A104

label=정화 장치 복구

conditions=인장 보유; 현장 작업 180분; 장치 부품 회수

effect\_ids\_json=\["EFF\_A104\_PURIFY"]

outcome=purify

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_A104\_PURIFY

operations=작업 완료 후 누출 중단 및 하루 오염 제거량 최대 8로 가동; 비휘발성 에덴 기록 획득; 전력·정비 필요

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_A104\_SEAL

event\_id=EVT\_A104

label=수로 동력 차단

conditions=차단기 접근 가능

effect\_ids\_json=\["EFF\_A104\_SEAL"]

outcome=seal

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_A104\_SEAL

operations=누출 중단; 농업 생산 승수 0.85를 2880분 적용; 기록 장치에서 에덴 자료 회수 가능

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_A105\_EVIDENCE

event\_id=EVT\_A105

label=확보한 증거 제출

conditions=보유 증거 1개 이상; 접수자 존재

effect\_ids\_json=\["EFF\_A105\_EVIDENCE"]

outcome=submitted

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_A105\_EVIDENCE

operations=증거 사본과 출처를 조사관에게 전달; 진범·무죄 판정은 증거 규칙으로 처리; 제출만으로 체포하지 않음

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_A105\_HIDE

event\_id=EVT\_A105

label=증거 은닉 시도

conditions=접근 가능한 증거 존재

effect\_ids\_json=\["EFF\_A105\_HIDE"]

outcome=conceal\_attempt

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_A105\_HIDE

operations=성공한 경우 접근성만 변경; 다른 목격자의 기억은 삭제하지 않음; 은닉 발각 시 별도 수사 근거 생성

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_A106\_KINGDOM

event\_id=EVT\_A106

label=왕국군 임시 관리 지지

conditions=왕국군 대표와 합의 가능

effect\_ids\_json=\["EFF\_A106\_KINGDOM"]

outcome=kingdom

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_A106\_KINGDOM

operations=관리권 왕국군; 영향력 왕국군 +15 및 타 세력 비례 차감; 지원 병력은 지역 병력 4명 실제 이동 예약

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_A106\_MERCHANT

event\_id=EVT\_A106

label=상인연합 가동 계약 지지

conditions=상인연합 대표와 합의 가능

effect\_ids\_json=\["EFF\_A106\_MERCHANT"]

outcome=merchant

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_A106\_MERCHANT

operations=관리권 상인연합; 영향력 상인연합 +15; 장치 수익의 20%를 계약 종료까지 연합에 지급

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_A106\_RAVEN

event\_id=EVT\_A106

label=기록을 주민에게 공개

conditions=에덴 기록 사본 보유

effect\_ids\_json=\["EFF\_A106\_RAVEN"]

outcome=public

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_A106\_RAVEN

operations=관리권 주민 공개위원회; 회색 까마귀 영향력 +10; 지식은 전달 경로를 따라 확산; 왕국군은 독립 조사 목표 추가

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_A106\_INDEPENDENT

event\_id=EVT\_A106

label=주민 공동 관리 제안

conditions=레나 평판>=10; 주민 대표 합의

effect\_ids\_json=\["EFF\_A106\_INDEPENDENT"]

outcome=independent

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_A106\_INDEPENDENT

operations=관리권 레나 공동체; 사기 +8; 외부 지원 자동 제공 없음; 세력 영향력은 현재 값 유지

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_A107\_DEPART

event\_id=EVT\_A107

label=세계탑 방향으로 출발

conditions=세계탑 좌표를 읽었음; 이동 가능

effect\_ids\_json=\["EFF\_A107\_DEPART"]

outcome=depart

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_A107\_DEPART

operations=세계탑 변경을 지도에 표시; 출발 이동 목표 설정; 레나 시뮬레이션 지속

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[CHOICE]

id=CHO\_A107\_STAY

event\_id=EVT\_A107

label=레나에 남아 준비

conditions=기록 접수 가능

effect\_ids\_json=\["EFF\_A107\_STAY"]

outcome=stay

selection=플레이어의 명시적 입력; 조건 불충족 시 비활성 사유 표시



\[EFFECT]

id=EFF\_A107\_STAY

operations=세계탑 좌표와 접근 열쇠 보존; 출발 기한 없음; 마을 시간은 계속 흐름

commit\_policy=실행 시 선행조건 재검증; 즉시 변경과 지연 작업 예약을 분리; 지연 작업 성공 전 보상 금지

idempotency=event\_instance\_id + choice\_id + effect\_id + operation\_index





\[EFFECT]

id=EFF\_DEATH\_COMMIT

operations=alive=false; death\_processed=true; current\_action 취소; 작업·거래 예약 해제; 소유 물품을 새 사체 컨테이너로 이전; 소속 마을 population -1; 노동 인원 -1; death fact와 사체 ID 생성

atomicity=죽음 fact\_id당 1회; Attack·Kill·질병·기아가 같은 프레임에 발생해도 중복 감소 금지

exceptions=플레이어 죽음은 설정된 게임오버·체크포인트 로드 정책 사용; 전체 스냅샷 복원으로 세계 상태도 일관되게 복원; 야생동물은 종 개체수만 감소

notification=주변에서 지각한 개체에만 정보 생성; 친족에게 전역 즉시 통보 금지



\[CRIME\_RULE]

id=CRIME\_RULE\_001

truth=행위 사실은 세계 원장에 기록; NPC 지식과 분리

witness=시야·거리·빛·소리·변장·기존 친숙도에 따라 행위 관측 확신과 신원 확신을 별도로 저장

hearsay=전언에는 원출처와 전달 경로 보존; 같은 소문을 여러 번 들어도 독립 증거 수 증가 금지

report=Report 실행 후 접수자가 받은 시점에 사건 접수; 도중 사망·위협·도주하면 접수 지연 또는 실패

investigation=관측 신뢰도와 물증 조합; 0.4 미만 정보 보류, 0.4\~0.8 조사, 0.8 이상 집행 검토라는 게임 규칙 제안

judgment=정당방위·오인·고의·우발성·소유권을 함께 평가; 수치 하나로 모든 법 판단 대체 금지

animal\_case=야생동물의 포식은 인간 살인죄가 아님; 가축 소유권 침해나 사람의 유도 행위는 별도 조사

security\_effect=확인된 폭력 사건은 치안 압력 상승; 수사·보호·배상 성공은 신뢰 회복; 같은 사건으로 매 틱 치안 중복 감소 금지



\[MEMORY\_TEMPLATE]

id=MEMTPL\_LOSS

fields\_json=\["owner\_id","deceased\_id","suspect\_id","source\_fact\_id","learned\_world\_minute","confidence","identity\_confidence","intensity","last\_recalled\_minute"]

effect=유대 70·슬픔 80이면 Mourn +35, Help +10; 가해자 확신 낮으면 Investigate +20; 확신 높은 원한은 추적·신고 목표에 기여

revenge=원한·충성·공격성·처벌 경험으로 복수 목표 우선도 계산; 공감·공포·법 준수·화해가 억제; Kill gate와 독립

recovery=장례·동료 지지·배상·시간 경과로 감정 강도 감소; 사실 기억은 별도 유지



\[SCENARIO]

id=SCENARIO\_DEATH\_CHAIN

example=에드윈이 원거리 도적 충돌에서 치명상을 입고 사망한 경우

flow=EVT\_SYS\_DEATH → 실제 목격 주민에게 정보 → 주민이 Flee 또는 Report 선택 → 다리안에게 신고 도착 → 수사 개시 → 리아에게 소식 전달 → MEMTPL\_LOSS 인스턴스 생성 → Mourn 또는 조사 목표 → 창고 책임자 승계

settlement\_effect=레나 인구 84→83; 에드윈이 맡던 관리 작업 중단; 임시 책임자 지정까지 배급 지연 가능; 자동으로 창고 식량을 소멸시키지 않음

story\_effect=EVT\_A101 협상자는 승계자 또는 창고 게시판으로 대체; 에드윈과의 개인 대화 및 관계 보상은 상실

no\_witness=시체가 발견되기 전에는 실종으로 알려질 수 있음; NPC 지식상 실종과 세계 원장의 사망을 분리



\[ITEM]

id=ITEM\_COIN

name=은화

unit=개

transfer\_rule=지불자 차감과 수취자 증가를 한 거래로 기록



\[ITEM]

id=ITEM\_FOOD

name=표준 식량 배급

unit=성인 1일 배급량

transfer\_rule=창고·화물·개인·사체 컨테이너 사이에서만 이동; 생산·소비·부패는 별도 원장 사유



\[ITEM]

id=ITEM\_EDEN\_MARK

name=에덴 인장

unit=개

story\_role=주인공 손목의 생체 열쇠; 거래·절도 불가; 수로와 기록 장치에 접근하는 경로 중 하나



\[ITEM]

id=ITEM\_EDEN\_RECORD

name=에덴 비휘발성 기록

unit=사본

story\_role=원본 훼손 시 수로 벽면과 중계소에 보존된 단편으로 핵심 좌표 재구성; 개인 기억 전체 복원은 보장하지 않음



\[RUNTIME\_SCHEMA]

id=SCHEMA\_RUNTIME

entity\_ids=동물 WOLF\_000001 등에도 species\_id·상태·위치·기억을 부여; 인간 비명명 주민 NPC\_GEN\_000001 등

fact\_fields\_json=\["id","kind","world\_minute","actor\_id","target\_id","location\_id","intent","cause\_event\_instance\_id"]

evidence\_fields\_json=\["id","fact\_id","observer\_id","observation\_confidence","identity\_confidence","source\_evidence\_id","created\_world\_minute"]

action\_fields\_json=\["entity\_id","node\_id","target\_id","started\_minute","progress","reserved\_resources","rng\_counter"]

shipment\_fields\_json=\["id","origin\_id","destination\_id","cargo","owner\_id","route","departure\_minute","arrival\_minute","status"]

ledger\_fields\_json=\["id","world\_minute","resource\_id","from\_id","to\_id","quantity","reason","idempotency\_key"]

job\_fields\_json=\["id","actor\_id","kind","target\_id","duration\_minutes","progress","completion\_effect","cancel\_policy"]

flags=수로 작업·관리권·기록 획득·좌표 확인은 저장 가능한 키와 값으로 보관; 서술 문장을 상태 판정에 사용하지 않음

sql\_mapping=정적 레코드는 type별 테이블; 가변 json은 초기에는 JSON 컬럼 가능; 관계·기억·선택·효과·원장은 독립 테이블 권장

schema\_limit=본 TXT는 데이터와 처리 계약을 제공하며 조건식 파서, 게임 로직, SQLite 실행문 자체는 포함하지 않음



\[VALIDATION\_RULE]

id=VALIDATION\_001

checks=전역 ID 중복 없음; WBG\_NODE 정확히 39개; 모든 선택→이벤트·효과 참조 존재; 종 활성/비활성 목록이 전체 39개를 중복 없이 분할

resource\_checks=재고 음수 금지; 실제 자원 원장 수량 보존; 생산·소비·부패 원인 명시; 예약 재고와 가용 재고 분리

population\_checks=명명 개체와 코호트 합계 일치; 코호트 승격은 인구 증가 아님; 사망 중복 처리 불가

simulation\_tests=같은 seed로 근거리/원거리 경제 총량과 핵심 사건 조건 일치 확인; 전투의 세부 경로까지 동일할 필요는 없음

regression\_cases=리아 사망 전후 단서 접근; 에드윈 사망 시 배급 승계; 늑대 멸종 시 사냥 사건 만료; 신고 없는 절도에 원격 처벌 없음; 저장 로드 뒤 선택·사망 효과 중복 없음

numeric\_policy=지수는 변경 후 범위 clamp; 인구·자원은 반올림 대신 단위 규칙 사용; 소수 생산·소비는 잔여 누적값 보존



\[ID\_RULE]

id=ID\_RULE\_001

static=대문자 ASCII와 밑줄; WORLD\_세계, REG\_지역, SET\_정착지, FAC\_세력, SP\_종, NPC\_인물

behavior=WBG\_01\~WBG\_39를 고정; 영어 name은 외부 코드 매핑에 사용하며 재번호 부여 금지

events=EVT\_P001\~ 프롤로그; EVT\_A101\~ 초반 1막; EVT\_SYS\_이름 시스템 사건

children=CHO\_이벤트숫자\_의미; EFF\_동일선택접미사; MEM\_기억; REL\_발신자\_수신자; MEMTPL\_런타임 기억 템플릿

runtime=EVINST\_<event\_id>\_<sequence>; FACT\_<sequence>; EVID\_<sequence>; BODY\_<entity\_id>\_<death\_sequence>; JOB\_<sequence>; SHIP\_<sequence>

uniqueness=런타임 sequence는 저장 파일별 단조 증가; 개체 ID 재활용 금지; 로드 시 counter 보존

reference\_policy=\_ids\_json은 ID 배열; 노드 name 배열은 WBG\_NODE.name을 참조; goal key와 flag key는 별도 열거형으로 등록

migration=정적 ID 삭제보다 deprecated 표시 권장; 버전별 이전 ID→새 ID 매핑 유지



\[RUNTIME\_ORDER]

id=ORDER\_001

step\_01=저장 또는 seed 로드 → ID·참조·형식·개체 수 검사 → 세계 시각과 난수 상태 복원

step\_02=일시정지 여부 확인 → 다음 예약 사건 또는 틱 경계까지 world\_time 진행

step\_03=같은 시각의 예약 완료·도착·환경 갱신 처리 → 자원 원장 커밋 → 읽기 전용 상태 스냅샷 생성

step\_04=근거리/원거리 전환 필요성 판정 → 기존 모드 작업을 경계까지 정산 → 진행률과 예약 자원 인계

step\_05=개체별 Evaluate → Observe → Remember → 종별 활성 및 실행 조건 필터 → Decide에서 가중치 합산

step\_06=행동 의도 수집 → 같은 자원·타깃의 경합을 시각과 고정 ID 순서로 해결 → 자원 예약 → 행동 진행

step\_07=피해·부상·사망 및 자원 이동을 원자적으로 확정 → 사실·증거 생성 → 관측 가능한 개체에만 전달

step\_08=NPC 기억·감정·관계·목표 갱신 → 신고 도착과 수사·애도·승계 예약; 같은 틱 무한 재진입 금지

step\_09=사건 trigger/conditions 검사 → 활성 시각·deadline 기록 → 도착한 플레이어 선택 또는 기한 자동처리 적용

step\_10=선택 효과의 idempotency 확인 → 지연 작업 등록 → 후속 사건 큐에 추가; 즉시 후속은 다음 마이크로단계에서 처리

step\_11=해당 주기이면 마을 배급·경제·치안·이주와 생태계 갱신 → 상태 변화가 만드는 새 사건 검사

step\_12=범위·보존량·사망 중복 검증 → 원장·이벤트 큐·난수·시간 부채 포함 저장 → 다음 시간 경계로 반복

flow=세계 시간 → 환경·예약 작업 → 인지 → 가중치 판단 → 행동 → 사실·증거 → 기억·관계 → 스토리·선택 → 마을·생태 변화 → 저장 → 세계 시간

ordering\_note=같은 시각의 다단계는 phase\_index를 함께 기록하여 재현; 단계 이후 생성된 결과를 이전 단계에 소급 적용하지 않음

