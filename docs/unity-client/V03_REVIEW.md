# V03 지면 커버리지 독립 정적 리뷰

2026-10-18 KST. **현재 V03 기능 범위의 코드상 차단 문제 없음.** 배정된 범위는 고정 확장 지면·외곽 장식·재생성·충돌 없음 및 저장된 정적 근거다. 미술 완성도, 진행 중인 C05 UI/제품 연결, 최종 통합 Scene 실행은 이 판정에 포함하지 않는다. 이 문서만 작성했으며 Unity/Editor/렌더/입력/빌드/테스트는 실행하지 않았다.

## 확인 내용

- ApplyV03은 Edit Mode의 C02 Scene에서만 동작하며 C02World 아래 기존 V03FieldSurround를 제거한 뒤 한 root를 재생성한다. 원래 월드 TexturedGround renderer와 짧은 ridge만 숨기고 로그인 diorama를 건드리지 않는다. 전체 builder 적용 경로도 ApplyV03을 호출한다. C05 추가 호출/패널은 별도 통합 리뷰 대상이다.
- Zone1Meadow는 identity C02World 자식 아래 중심(50,-0.035,50), Plane scale(24,1,24)의240m 지면이며 X/Z[-70,170]을 덮는다. 새 collider는 Shape에서 즉시 제거한다. 기존 서버 좌표·이동 경계·카메라 추적·picking surface를 확장하는 코드가 아니다.
- 현재 저장 Scene의 V03 subtree를 파일로 집계해 Transform174/MeshRenderer125/MeshFilter125, Collider/MonoBehaviour/UI component0을 확인했다. root transform은 identity다. C05가 편집 중인 Scene의 다른 영역을 통합 검증했다고 주장하지 않는다.
- Meadow는 V03 경로의 기존 EarthAndGrass 복제 재질이며 BaseMap scale3.75=240/64, offset0이다. 원본64m 지면과 새240m Plane의 중심/반전 UV 관계는 반복 주기64m를 유지하며 위상 차이가 정수 주기다. DistantRidge 역시 V03 복제 재질이다. 기존 V02 공유 재질을 덮는 경로는 발견하지 않았다. 외곽48 pine/24 rock/4 ridge는 기존 자체 저작 mesh를 재사용한다.
- `v03-static/coverage.txt`는 실제 카메라의 중앙(39,39) 및 Zone1 네 모서리의 viewport20 지면 교점이 모두 확장 범위 안임을 기록한다. 고정 orthographic 방향/크기의 평행 이동에서는 이 사각형의 극값 검사로 내부 지면 커버리지를 설명할 수 있다. 현재1280×720/16:9 외 화면비의 보장은 아니다.
- 저장된 중앙 및(105,105) 정적 PNG를 직접 읽어 지면 끝/빈 배경 노출이 없음을 확인했다. 넓은 영역이 반복 녹색 지면 위주로 비어 보이는 미술 한계는 남는다. Coordinator가 추가 미술을 계획 중이며 밝은 판타지 MMORPG 미술 완성도를 수용했다는 판정은 하지 않는다. PNG는 합성 Edit Mode 카메라 검사이고 서버 이동/로그인 통과 근거가 아니다.
- 네 C04 Prefab 해시는 기존 동결과 동일하다. 서버/DB/wire/AI/충돌·내비게이션/가짜 데이터·예비 UI 기능 추가는 V03 영역에서 발견하지 않았다.

## 파일 기준과 한계

아트 동결 기록 `Captures/v03-art-freeze.txt`의 Builder `514C4A950DB25D7BDA3A231FCC6FE853AADF610C218A86EF9C778141B67D0172` / Scene `AF841AD9AF9D5BD6F5EAFFD5F05CC36E2CCA047AC1AAB532025C98C5E8BF0AD1`는 **V03 인계 당시** 기준이다. 이후 C05 담당이 같은 Builder/Scene을 인계받아 편집 중이며, 이번 읽기 시점 실제 전체 해시는 Builder `8BDD31CF64E3D640025AB32CA1ABF41B2DFDC5FA9842E1B7492102B4DC5AA004` / Scene `8328A40A724E6C4E9ACEA56E155632DA2F9FA3CDE3F352599B096A3F9C4529CF`다. 이 전체 파일을 최종 동결·통합 리뷰한 것으로 보고하지 않는다. V03 코드/자산/subtree만 범위에 포함했다.

직접 일치 확인한 V03 자산 SHA256:

| 파일 | SHA256 |
| --- | --- |
| Meadow.mat | `8D2A7BB940E4DE4449955ED9F3FA8A1B4669CFCFB4B69D3491BC029C5C99D729` |
| DistantRidge.mat | `752F9E3988C0F7E2A99C57FEE6B251E743C46306C32ED35D3DAF0876BBD37D4E` |
| README.md | `08D0ABFF49C4D9192A4DB3644E735F1D26293AB3BA21A4B4DC5C6ED21496CADD` |

담당 기록의 compile completed/failedfalse/errors[]·재적용 root1·missing reference0·world inactive/dirtyfalse는 아트 인계 시점 근거다. 리뷰어가 재실행한 검증은 아니다. C05 통합 동결 후 실행 배치와 이후 추가 미술은 별도 검증·해시 기준을 필요로 한다. 문서 작성 지연 때문에 독립 코드 작업을 중단하도록 요구하지 않았다.
