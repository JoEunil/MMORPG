# 사냥 화면 피드백 묶음

목표: 기존 몬스터 HP바에서 서버가 확정한 HP 감소를 짧게 강조해 피격을 읽기 쉽게 한다. 필드 진입로 보강과 함께 사냥 화면 개선 묶음으로 모으며 작은 PR을 만들지 않는다.

계약: C04ObservedWorld의 기존 몬스터 Delta 배치 검증·HP0 제거가 우선한다. 알려진 살아 있는 몬스터의 유효한 HP 감소를 반영한 뒤만 금색 강조를 요청한다. Full·이동·동일 HP·무효/미지 ID·시전 요청은 강조 근거가 아니다. 피해 숫자나 공격자를 추정하지 않는다. 실제 HP바 길이는 서버 상태로 즉시 갱신하고 강조색만 unscaled 0.2초 뒤 복구한다. disable/제거 시 효과를 남기지 않는다.

소유: 구현 담당은 candidate C04ObservedWorld.cs, C13EntityVitals.cs, 기존 C04ObservationTests.cs만 수정한다. Coordinator가 ROOT 동기화·Editor·검증을 맡는다. 다른 Scene·에셋·설정·서버·DB는 변경하지 않는다.

최소 검증: 구현 동결·독립 리뷰 뒤 기존 관측 fixture 최대2개로 감소/비감소·잘못된 패킷·정리 경계를 확인한다. 중간에는 컴파일만 확인한다. 실제 새 강조 효과의 사냥 화면 확인은 관련 사냥 화면 묶음의 입력 배치에 합치며 기존 X/C 피해·전체 로그인 회귀를 별도로 반복하지 않는다. 단순 기획/코드 완료를 실서버 시각 검증 완료로 표시하지 않는다.

상태: 로컬 구현·독립 리뷰·최소 자동2개·실제 한 접속 배치 완료. 결과와 제한은 MONSTER_FEEDBACK_RESULTS.txt를 따른다. 서버 변경 없음. 승인된 UI01은 별도 동결 범위이며 이 기능은 포함하지 않는다.

## 원형 공격 예고 추가 범위

일반/강한 몬스터의 기존 Circle 스킬 0/1만 예고한다. 서버가 보낸 casterType1/casterId=internalID(0 포함)와 고정 x/y를 사용한다. 종류0은 skill0, 종류1은 skill0/1만 허용하며 보스3/4·미관측·무효 데이터는 제외한다. C14ServerContent의 실제 Hit phase Circle 반경·waitTicks를 읽고 시간은 50ms tick 기준 표시 정리 여유0.1초만 더한다. 서버 시각/castID가 없으므로 정확한 판정 카운트다운으로 설명하지 않는다.

phase0에서 빨간 원 윤곽을 표시하고 phase1/HP0/관측 제거·만료/Zone·모델 교체/disable/TTL에 정리한다. 원은 움직이는 몬스터 보간 위치를 따라가지 않고 서버 시전 좌표에 고정한다. 빌드 참조가 있는 전용 Material을 사용하고 runtime Shader.Find나 새 Material 생성은 하지 않는다. 기존 관측 source/generation 가드를 재사용한다.

범위 확장 소유는 C04/C13/기존 관측 fixture와 C02SceneBuilder의 BindC04 참조 연결뿐이다. root만 새 Material/meta와 Scene 참조2개를 작성한다. 기능 실행은 두 변경을 모두 동결·독립 검토한 뒤 MonsterFeedback_ 시나리오2개와 한 실제 접속 배치로 끝낸다. 배치 중 Assets/Packages/ProjectSettings 쓰기를 중단한다. 실제 입력은 가까운 일반 몬스터에 X1회와 몬스터 phase0/1·HP 강조 관측만 수행하며 구매·다계정·이전 전체 회귀는 제외한다.
