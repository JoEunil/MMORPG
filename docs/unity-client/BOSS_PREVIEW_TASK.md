# 보스 부채꼴 공격 예고

목표: 기존 보스(type2) skill3의 두 Charging→Hit 구간을 서버 판정과 같은 월드 고정 부채꼴 윤곽으로 표시한다. 일반 몬스터 Circle과 피격 강조를 유지하고 사냥 화면 묶음으로 전달한다. 새 스킬/수치/패킷/리깅/DB 변경은 없다.

계약: casterType1/internalID/slot0/skill3, C14의 해당 Charging 다음 Hit에서 반경·waitTicks·Boss1_1/Boss1_2를 읽는다. Boss1_1은30~150/210~330도, Boss1_2는330~390/150~210도, atan2의 X/Z 월드 기준이며 dir로 회전하지 않는다. range2는 서버 판정에서 사용하지 않으므로 폭으로 해석하지 않는다. phase0/2 수신 때만 해당 예고를 만들고 phase1/3에서 대응 구간만 제거한다. 전체4phase가 실제 지원 계약과 다르면 표시하지 않는다. skill4는 미지원이며 다른 스킬의 Hit가 skill3 예고를 제거하지 않는다.

수명: 서버 고정 x/y. 두 예고 구간은 독립적으로 보존하여 저HP 보스의 겹치는 시전에 대응한다. castID가 없고 서버시각도 없으므로 완전한 시전 취소/식별이나 정밀 카운트다운을 주장하지 않는다. TTL=다음 Hit waitTicks×0.05+0.1초, HP0/관측제거/모델·Zone전환/disable정리. 기존 source/version/WorldAvailable 검사 재사용.

소유: 구현자는 CAND C04ObservedWorld.cs 및 기존 C04ObservationTests.cs만 수정한다. ROOT 동기화/Editor는 Coordinator만. Scene/Material/콘텐츠/서버/다른 파일은 변경하지 않는다. 다른 담당자 변경을 보존한다.

최소검증: 동결·독립 리뷰 뒤 컴파일 및 기존 관측 fixture에 새 직접 시나리오1개만 추가해 두 모양의 좌표·방향무관·고정중심·중첩/대응 Hit·skill4무시·수명정리를 묶는다. 일반몹 자동2개/기존 로그인 회귀를 재실행하지 않는다. 실제 보스 화면은 다음 서버 교체 관련 입력 배치에 합치며 자동 성공으로 실제 화면 완료를 대신하지 않는다. 공개 사이트는 실제 캡처 확인 뒤 갱신한다.