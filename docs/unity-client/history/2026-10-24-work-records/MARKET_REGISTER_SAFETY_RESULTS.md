# 등록 실패 처리 안전 보완 기록

이번 후보는 기존 내부 등록의 불명 DB 결과를 실패로 단정해 아이템·수수료를 중복 반환하는 경로와 worker 무응답·메시지 소유권을 보완한다. 등록 화면·외부 TCP는 추가하지 않았다. 기존 수수료와 DB 스키마·SQL·WAL/flush 구조는 그대로다. 전체 등록의 자원 원자성·재시도 멱등성·재시작 복구는 여전히 미해결이며 외부 등록 공개 조건이 충족된 것으로 보지 않는다.

SQL이 1행을 반환하면 성공1, 명시0이면 기존 보상을 한 번 시도한다. SQL 예외·예상 밖 반환은 결과미확인5이며 자동 보상하지 않는다. 실행 전 큐 거절은 보상을 시도하고 어느 자원이라도 보상 실패·부분 예외가 있으면5로 표시한다. 수수료 차감 실패 뒤 아이템 복원이 실패한 경우도5로 구분한다. 성공을 확인하지 못한 요청을 자동 재등록하거나 자원을 자동 조정하지 않는다.

요청 값을 worker 경계 전에 복사하고, 등록 요청만의 공유 수명과 단일 종결 guard로 응답 enqueue 시도·Message Return을 한 번 처리한다. 응답 큐 거절/예외는 전달 성공을 뜻하지 않는다. 내부 요청 길이도 검증한다. 공통 worker/큐 구조는 변경하지 않았다.

독립 리뷰에서 invalid item/quantity0 응답이 예외 처리 밖에 있어 응답 큐 예외가 Cache MQ thread를 종료시킬 수 있는 P2를 발견했다. invalid 응답도 기존 try 안으로 옮기고 기존 두 번째 fixture에 invalid+응답큐 예외 조합을 포함해 수정했다. 독립 재검토에서 차단 문제 없음. 최종 Release MainServer/UnitTests 증분 빌드0, 직접 native2/2 통과(48ms)했다. 기능 검증은 동결된 최종 버전에 한 번만 실행했으며 초기·수정후 빌드와 리뷰를 기능시험 통과 횟수로 세지 않는다. root UnityClient/Builds/Market-Register-Safety/build-final.txt와 UnityClient/Captures/market-register-safety-native.txt에 근거를 보존했다.

검증 범위는 제품이 직접 사용하는 SQL/보상 helper와 실제 Handler+stopped DBWorker fixture의 두 경계다. queued worker 실행·Acquire 실패·onFailure 재진입과 fee 실패 후 실제 inventory 복원 BLOCKED는 실행하지 않으므로 정적 검토와 구분한다. 모의 SQL 예외는 실DB COMMIT·crash 검증이 아니다. 기존 Unity/시장 조회·구매·취소 입력과 캡처는 반복하지 않는다.

실행 서버와 DB·계정은 유지했다. DDL·seed·실등록·강제 종료·서버 재시작·Unity 에셋 변경은 없다. 화면과 지원 기능이 바뀌지 않았고 공개 페이지가 이미 거래 등록 미지원으로 설명하고 있어 이번 내부 보완을 기능 구현으로 게시하거나 대표 캡처를 교체하지 않는다. 원본 Game/root WIP와 pending catalog를 보존한다.

게시 대기 거래소 후보 f2cfb34의 MARKET03은 이 등록 보완을 포함하지 않는다. 승인 원문을 추가 범위로 확대 해석하지 않으며 로컬 후보와 게시 범위를 분리한다.
