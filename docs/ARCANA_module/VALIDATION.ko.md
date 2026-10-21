# 원본 DB 통합 검증 v1.1

검증일: 2026-10-15 (Asia/Seoul)
환경: Windows x64, Visual Studio 2022 / MSVC 19.44.35215.0, CMake 3.31.6, Release, C++17, SQLite 3.54.0.

## 최종 결과

- 최종 소스 전체 빌드 성공.
- CTest 3/3 통과: 기존 범용 simulation, 기존 content, 신규 native.
- 원본 DB와 실행용 복사본 모두 native 예제 정상 실행: nodes=39, entities=9, territories=1.
- 원본 DB: 40개 테이블, integrity_check=ok, foreign_key_check=빈 결과.
- 사용자 Downloads 원본과 배포 DB의 SHA-256이 최초 확인값과 동일함.
- 첨부 build.py와 원본 SQL 쓰기 예제는 실행하지 않음.

## 신규 native 검증 항목

1. 실제 39개 노드의 ID/영문명, 4개 프로필, 9개 개체, 일반 동물/마수 구분을 읽음.
2. WBG 효용 선택, 최소 2초 유지, hysteresis 0.15, 화재 중단, 대상 무효 시 재선택.
3. 원본 침공·파견 경계값, 상호 동의·혼인 자격 조건.
4. 원본 마력 공식, 0 분모 방지, 반복 사냥 감쇠.
5. 원본 DB에 RuntimeStore 쓰기 거부.
6. 실행용 복사본에서 사망 기록의 멱등성, 혈통/전투 기여/흡수 거리/60초 제한.
7. 흡수 후 mana_max만 증가하고 mana_current 유지; 중복 요청 시 재지급 없음.
8. 행동 저장·재로드, 시간이 뒤로 가는 쓰기 거부.
9. 흡수 로그 INSERT를 실패시키는 테스트 트리거를 주입하여 최대 마력 갱신 롤백 확인.
10. 원본을 변경하지 않는 실행용 복사 도구, 기존 대상 파일 덮어쓰기 거부.

## 원본 SHA-256

`7f42a8c4c0196af0fe30332b36ad305ce29f213e2d3a9595d43525c768b77cb3`

## 검증 범위 밖

기존 OpenGL 게임 소스가 제공되지 않아 실제 렌더러/AI/NavMesh/전투/저장 연결 실행은 하지 못함. Host의 실제 39개 행동 실행, 전체 사회·경제 사건 처리, 집계 LOD, 대규모 성능, 다른 운영체제는 별도 구현·검증이 필요함. 예제 Host는 Idle/Wander만 지원하며 원본 DB 데이터 자체를 수정하지 않음.
