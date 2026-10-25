# 매물 등록 실패 처리 보완

2026-10-20 계속진행. codex/market-register-safety는 게시대기 f2cfb34에서 분기하며 MARKET03 승인에 추가 변경을 포함하지 않는다. 기존 등록은 cache item/gold 차감과 매물 INSERT가 서로 다른 영속경계라 외부 UI/TCP 연결만으로 안전하지 않다. 이번 묶음은 기존 내부 등록의 불명 결과 자동보상과 무응답/메시지 소유권 결함을 좁게 고친다. 완전한 등록 원자성·재시도 멱등성을 해결했다고 주장하지 않는다.

범위: SQL 시작 뒤 예외/불명 결과는 내부 status5(결과미확인)와 수동조사 로그로 종결하고 자동 아이템·수수료 복원을 하지 않는다. SQL 미실행이 확실한 queue 거절과 명시적 삽입실패는 기존 보상을 시도하되 보상 실패는 status5로 분리한다. 요청은 비동기 경계 전에 값복사하고 응답·Message Return을 정확히 한 번 처리한다. 기존 정상 등록·수수료 수식·DB 스키마·SQL/procedure·WAL/flush 구조는 보존한다. 새 외부 opcode/등록 UI·범용 transaction 프레임워크는 추가하지 않는다.

소유권: 서버 분석은 MARKET_REGISTER_CONTRACT.md만, 구현 담당은 CacheLib/Handler_Bazaar.cpp·필요한 최소 helper와 CoreLib/MessageTypes.h 상태주석·기존 native fixture의 직접 영향 검증만, Coordinator는 작업/결과/인계·Git·실행환경이다. 독립리뷰는 읽기만 한다. 원본Game/rootWIP와 pendingcatalog를 보존한다.

검증은 동결·독립리뷰 후 자동2경계 이내: SQL 시작 뒤 결과유실/예외에서 보상0·status5와 정확1종결, 실행 전 실패/명시0에서 보상성공·실패의 결과와 정상1 유지. 실제공용DB·계정·서버를 조작하지 않는 fault fixture로 확인한다. 새 UI·wire가 없어 Unity 로그인·입력·캡처/사이트 재촬영은 반복하지 않는다. 서버후보빌드와 직접 native검증만 수행하고 실제서버는 유지한다. 신규SQL·DB테이블이 없으므로 로컬DB DDL/seed0이다.

검증된 후보를 로컬커밋하고 기존거래소와 큰 단위로 게시 준비한다. PR직전 Slack 게시 승인만 요청하며 이미 게시대기중인 정확후보와 승인범위를 섞지 않는다.
