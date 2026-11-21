# 성장 데이터

`Growth.csv`는 UTF-8 CSV이며 첫 행은 `level,requiredTotalExp,maxHpIncrease`다. 열 순서는 바꿀 수 있다.

- `level`: 기존 상한인 100레벨까지 1부터 빠짐없이 오름차순으로 정의한다. 중복·누락은 오류다.
- `requiredTotalExp`: 누적 경험치다. 1레벨은 0이며 이후에는 반드시 증가한다.
- `maxHpIncrease`: 해당 레벨에 새로 도달했을 때의 최대 HP 증가량이다. 1레벨은 0이며 값은 `INT32_MAX` 이하여야 한다.

빌드 시 실행 파일 옆의 `Data/Growth`로 자동 복사한다. 서버는 시작 시 한 번 읽고 검증하며, CSV 변경은 재빌드·재시작 후 적용된다. 저장된 캐릭터의 레벨과 HP를 소급 재계산하지 않는 기존 정책은 유지한다.
