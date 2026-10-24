# C03 — 단일 Zone 클릭 이동

목표: C02 실제 Zone+본인 Full 진입 뒤 바닥 클릭 지점으로 서버 기존 이동 흐름을 사용해 걷는다. 서버 좌표를 기준으로 위치/HUD를 갱신하고 시각 보간과 구분한다. V02 대표 외형/UI를 보존한다.

현재 단계: C03_CONTRACT.md 분석 및 C03_SPEC.md 수용 조건 확정 후 최소 구현. Unity 입력 테스트는 계속 보류한다. 새로운 실행 통과를 주장하지 않고 검증 항목은 INPUT_TEST_BATCH.md에 모은다. 현재 아트/Editor 조작 담당은 없으며 컴파일·정적 리뷰만 즉시 수행한다.

포함: 본인/한Zone/지면클릭·연속 이동 요청·서버 확정 동기화·UI/입력focus 차단·연결해제/컴포넌트비활성/Zone 변경 정리. 현재 ClientCore 네트워크/ViewModel/ViewData/dispatcher 경계 재사용.
제외: 타플레이어·몬스터·전투/스킬·새지형충돌/패스파인딩·Zone이동 자동화·새wire/서버속도나판정변경·DB/운영/유료에셋·범용Manager/DI/이벤트버스.

계약: 기존 소스 실제 wire와 실행 경로를 C03_CONTRACT.md에서 확정한다. enum만으로 지원을 추측하지 않는다. 서버/새프로토콜/보호영역 변경은 후보를 준비해 Coordinator만 Unity Coordinator 앱의 변경별 Slack 스레드로 승인 요청하고 승인 전 수정하지 않는다. 일반 ClientCore Unity 최소 호환 범위는 기존 확정 권한을 따른다.

소유권: 서버 계약 담당 C03_CONTRACT.md/요청 후보만, 기획 담당 C03_SPEC.md만, root C03_TASK.md/배치/Slack/진행기록. 구현 파일은 계약 확정 후 한 담당자에게 명시 배정한다. 고정 diff의 독립 리뷰는 별도 담당. 같은 파일과 Editor를 동시에 조작하지 않는다.

검증 목표/비용/종료: 컴파일·패킷/계산/수명 경계 관련 최소 비입력 검증·독립 리뷰. 실제 UI 흐름은 B01 로그인/선택/진입에 이어 지면 클릭→여러 서버 좌표 업데이트→목표 근처 정지, UI/포커스 차단·연결 정리를 B02로 배치. 코드/리뷰 완료와 입력 미실행을 구분한다. 같은 원인 두 번 수정 뒤 재현/근거를 Coordinator에 넘긴다. 입력 재개 지시 전 Unity Play/입력/상태 주입/실행 캡처는 하지 않는다.

계약 확정/Coordinator 수용: C03_CONTRACT.md 실제 ACTION18/Full43/Delta10과 기존 UpdateMove API 일치, 서버 변경 없이 구현한다. 보호 영역 승인 요청은 현재 필요 없다. 이동 수신 순서/ACK 없음·방향의 전투 경로 불일치 한계는 유지한다.

구현 소유권: c03_implementation은 C02WorldScreen.cs, ClientCore/ViewData.cs·ViewModel.cs·Services/NetworkService.cs와 단일 원본 빌드 DLL, 필요한 최소 C03 순수회귀테스트/메타 및 c03-implementation.txt만 수정한다. 네트워크수명 변경을 포함해 구현 역할 지침의 Sol/high를 적용했다. Scene/builder/Art/C01/기존테스트·서버·패키지/설정 변경은 제외한다. 추가 API/직렬화 변경이 필요하면 root에 최소안을 먼저 보고한다. Editor는 해당 담당이 status/refresh/compile/정적확인만 단독 소유하며 Play·입력·상태주입·실행캡처는 하지 않는다.

구현 기준: 최신 확정 위치에서 후보거리≤1, 송신 후보주기≥50ms, 긴dt 누적/캐치업 없음. root Transform/서버좌표 HUD는 확정값이며 기존 C02 helper 조건을 보존한다. 입력 UI raycast/텍스트포커스 차단,유한 목표타임아웃/수명정리. 최초 skill255와 유한성·dir 경계 및 취소/송신실패 전파만 ClientCore 최소 호환으로 수정한다. 새 wire/서버 정책·시퀀스/ACK 추가는 하지 않는다.

Slack 변경 스레드: Unity Coordinator 앱 C03 / 1792310203.712469. 승인 필요 범위가 새로 드러나면 정확 파일·동작·검증·복구 후보를 준비해 이 변경의 요청으로 연결한다.

최소 경계 추가 수용: 기존 이동 입력 경계인 IViewDataUI.UpdateMove를 재사용하기 위해 C01LoginScreen.cs에 Data => client?.GetViewData() property1개, Interfaces.cs의 IViewDataUI.CancelMove()1개와 ViewData 구현을 허용한다. ViewModelUI에 전달용 이동함수2개를 추가하지 않는다. C02.Runtime.asmdef에 기존 Unity.InputSystem 참조1개를 허용한다. Scene/직렬화/새인터페이스·wire변경은 없다. C02EntryStateFixtureTests.cs의 기존 불가능한 성공 합성(Zone5/범위밖좌표)은 실제Zone/유효좌표와 동일기대결과로 최소 조정하고 invalidZone/좌표의 별도거부 검증을 추가한다. assertion 삭제/완화나 실제 UI helper 변경은 허용하지 않는다. 원본 ClientCore Unity 최소호환 확정권한 안의 좁은 API/수명 수정이며 서버 보호영역 승인은 필요 없다.

리뷰 수정 결정: 대각 동률은 확정 SPEC대로 X축 우선(초기 구현보고 Y는 대체됨). V02 영웅이 local -Z를 바라보므로 서버dir0/+Y→yaw180,1/-Y→yaw0,2/-X→yaw90,3/+X→yaw270으로 시각회전을 맞춘다. 비상호작용 WorldHud 패널 자체도 RectTransform 영역검사로 입력을 차단한다. SendUpdate는 session lock 안에서 상태 스냅샷만 취하고 network.Action은 lock 밖에서 실행해 socket/session 역순을 없앤다. 이미 소비/전달된 한 segment 취소불가 한계는 유지한다. 기존 숨김 FieldPlane collider와 V02 고정 TexturedGround는 별개로 시각지면이 따라 미끄러지지 않음을 리뷰에서 확인했다.

첫 고정본은 컴파일0·Edit Mode 직접 순수5개 통과 이력이지만 위 수정 전 결과다. 최초 테스트의 internal PacketBuilder 접근 CS0122와 reflection 교정 및 에디터 포커스 대기/미로드type 조회 이력도 보존한다. 최종 수정본의 컴파일/6개 순수회귀·재리뷰를 별도로 기록한다. UnityNUnitRunner·실제 입력·서버이동 검증은 아직 미실행이다.

최종 상태: 코드·컴파일·정적 단계 수용, 실제 기능 입력 통과는 대기. 원본 Core빌드0오류(NU1900 metadata조회경고2), 최종Unity컴파일0오류, EditMode직접순수회귀6/6, 독립리뷰네지적해소/코드상차단문제없음. NUnitRunner/Play/실제UI·서버이동은미실행. 최종전체해시는 Captures/c03-final-freeze.txt/c03-implementation.txt, 실제배치는 INPUT_TEST_BATCH.md B01+B02 최신C03통합본 기준이다. root가Play/paused/compiling/dirty/runner/runInBackgroundfalse, 기존입력장치/서비스유지·gameTCP established0·원본Game변경보존을읽기확인했다. Editor조작담당없음. 서버/DB/새wire/운영/Scene/Art 변경·커밋·push·공개없음. 다음C04는계약조사완료/제품구현전이다.
