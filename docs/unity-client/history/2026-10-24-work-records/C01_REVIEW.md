# C01 독립 코드 리뷰

Editor-update 입력 runner 재리뷰: `C01EditorInputRun.cs` SHA256 `570F7D2B5A1E75191B76A201B9865D6E049CECD89BC4442156E963A4FFA4A20E`, 원본 UI coroutine은 `61299FF9...` 그대로다. 기존 테스트 coroutine을 reflection으로 얻어 중첩 IEnumerator를 실행하고 동일 입력·assertion을 통과해야 PASS한다. 제품 handler나 상태를 직접 설정하지 않는다. 120초 상한·Play 종료·예외에서 stack을 Dispose하고 Editor 이벤트를 해제하여 기존 finally의 장치/설정 복구를 실행한다. 출력은 별도 Editor-update 결과임을 명시하고 예외 메시지/스택·입력값을 기록하지 않는다. **코드상 차단 문제 없음**. 이 결과를 NUnitRunner 통과로 표시하면 안 되며 실제 실행은 복구 담당의 별도 1회 검증 대상이다. 리뷰어는 Editor/제품을 조작하지 않았다.

Focus/문자 도달 대기 delta 재리뷰 SHA256: `61299FF9D8565F655AC6675FD531D6E049F49116870938CFEFB6534169A93D71`. GameView Focus 뒤 실제 마우스 클릭과 focus/선택 검증을 수행하며 각 문자 KeyDown 뒤 최대 1초 동안 예상 prefix 도달을 관찰한다. prefix와 최종 입력 일치는 bool로만 assertion하고 진단에 입력값을 넣지 않는다. 제품 데이터/핸들러를 직접 변경하지 않고 동일 최종 입력·서버 합격 조건을 유지한다. **코드상 차단 문제 없음**; 승인된 격리 UI 실행 결과로 판정할 단계다. 제품/Editor 조작은 수행하지 않았다.

실행 전 Mouse bitfield 정정 재리뷰 SHA256: `D03F35F2BE7BD6717CD5D10DA6D5F4E141176966E35C96B8AD97212B7F5D4E6B`. 앞선 `75FCD375...`는 미실행이며 실행 기준이 아니다. 클릭 세 줄이 bitfield DeltaStateEvent에서 전체 MouseState QueueStateEvent의 위치 중립→왼쪽 누름→중립으로 바뀌고 각 프레임을 유지함을 확인했다. 임시 장치와 실제 UI 입력·동일 조건을 유지하며 **코드상 차단 문제 없음**. 이 버전의 실제 UI 통과는 격리 실행으로 확인한다.

Native runtime 보존 delta 재리뷰 SHA256: `75FCD37528E4C39491B16FD2DB5021C402326E3028CCC6DC995FF3D55E2B4FF5`. InputTestFixture 상속을 제거하고 임시 Mouse의 위치/누름/뗌 상태 이벤트를 프레임별로 공급한다. 임시 장치는 try 안에서 생성하고 finally에서 제거한다. GameView 문자 큐와 클릭 focus·문자 도달 bool·서버 결과 조건·비밀 없는 assertion을 유지하며 제품 handler/필드 직접 호출은 없다. **코드상 차단 문제 없음**; native 문자 도달 여부는 승인된 격리 1회 실행으로 확인한다. 제품/Editor는 수정·조작하지 않았다.

입력 복구 재리뷰 기준 SHA256: `116CDDBF570B66F9149A3C2891302F67401E7A25DB229E33BC0424B3C6D893A1`. InputTestFixture 초기화 뒤 항상 C01 Scene을 로드하고, 클릭의 실제 focus/선택 상태를 bool로 검증하는 delta를 확인했다. 진단은 정적 오브젝트 이름과 action 활성 여부만 포함하며 입력값을 출력하지 않는다. 입력 큐·문자 도달·서버 합격 조건을 유지하며 직접 handler/필드 우회가 없다. **코드상 차단 문제 없음**; 실제 입력 도달·UI 통과는 복구 담당의 격리 실행 결과로 확인한다. 제품/Editor 조작은 수행하지 않았다.

최신 테스트 도우미 재리뷰 기준: SHA256 `22E0C8751D09B1800B45881B21DC60DAB83C2980C5EA469A5D7349AFDC23F4F8`. QueueGameViewInputEvent 입력 및 기존 bool/서버 조건을 유지한다. 테스트 최상위 try 안에서 runInBackground를 임시 true로 바꾸고 finally에서 이전 값과 Editor 포커스를 복구하는 추가 변경을 확인했다. 제품 설정/Scene 수정은 없으며 **코드상 차단 문제 없음**. 격리 UI 1회 실행 가능한 코드 상태이고 실제 통과는 실행 근거가 필요하다.

2026-10-18 KST. 판정: **코드상 차단 문제 없음**.

- 범위: ClientCore 허용 7파일 diff, `Assets/C01/`, `Assets/Tests/C01/`, `Tools/Build-ClientCore.ps1`, DLL 및 `BuildProvenance.txt`. 기존 준비 파일은 제품 변경과 구분했다. 제품·테스트·Editor·서비스를 수정하거나 실행하지 않았다.
- 원본 ClientCore 7파일과 C01 제품 파일의 SHA256은 provenance 기록과 모두 일치했다. Release DLL과 Unity DLL의 SHA256도 일치한다. Unity에는 ClientCore 소스 복사본이나 추가 Json DLL이 없고 기존 Unity Json 패키지를 사용한다.
- 서버 소스의 인증 9바이트·목록 430바이트 및 count 최대 10, 빈 조회의 `status=0,count=0` 의미와 구현을 대조했다. HTTP 취소 뒤 세션 반영 방지, 연결별 수신 버퍼, 프레임 경계 검증, 부분 송신 반복, 종료/이벤트 해제와 메인 스레드 dispatcher 경로에서 C01을 차단하는 결함을 찾지 못했다.
- UI 테스트는 Input System의 마우스·키보드 입력을 사용한다. 내부 제품 핸들러를 직접 호출하는 목록 fixture는 별도 단위 검증으로 구분된다. 불필요한 새 계층·복제 상태에 관한 수정 요구는 없다.
- 미검증: 이번 리뷰는 정적 검토다. 컴파일 통과는 구현 인계 기록이며 리뷰에서 재실행하지 않았다. 실제 UI/서버·프레임 테스트 및 캡처는 테스트 담당의 실행 근거가 필요하다. UI 테스트의 scene reload는 Play 종료·재시작과 다르므로 별도 Play 재시작 관찰이 필요하다. 부분 송신 반복은 코드에서 확인했으며 부분 송신을 강제한 실행 검증은 없다. 실행 서버 바이너리와 서버 소스의 빌드 일치는 계속 미검증이다.

제품 변경이 생기면 해당 경로와 DLL/provenance 일치만 재검토한다. 코드 리뷰 판정은 실행 검증 통과를 대신하지 않는다.

## UI 테스트 강화 재리뷰

대상: `C01LoginFlowTests.cs` SHA256 `D35E3CF1C2FDD0D47E3E5ED9E514DF4F9355D96B1A2624B1DADC6CA8A07BF610`. 제품은 재수정하지 않았다. Press/프레임/Release 입력, 입력 텍스트 도달 bool assertion, 실패 상태 대기 및 마스킹된 diagnostics는 실제 입력 경로를 유지하며 assertion을 약화하지 않는다.

판정: **차단 문제 있음 — P2 테스트 실패 시 비밀번호 출력**. `AssertNoSecretDisclosure`의 `Assert.That(visibleText, Does.Not.Contain(account.Password))`는 비밀번호가 UI에 노출되어 assertion이 실패하면 NUnit의 expected constraint에 실제 암호, actual에 UI 원문을 남긴다. 비밀번호 노출 회귀 조건에서 실행 결과 파일·로그에도 비밀번호가 확산된다. 최소 수정은 비밀번호 포함 여부만 bool로 assertion하고 고정된 비밀 없는 실패 메시지를 쓰는 것이다. 실행은 이번 재리뷰에서도 수행하지 않았다.

### 최종 정정 확인

최종 테스트 SHA256: `18F992FB160BD3652DCAC902F06239E0DC4C19DCF54828191482CB7CFB095FA1`.
위 지적과 같은 함수의 비밀번호 필드 비움 assertion을 bool로 고쳤다. 상태/목록 원문을 actual로 출력하던 assertion도 같은 합격 조건의 bool로 바꾸고, 단계별 진단은 계정 ID·암호·JWT·GUID를 마스킹했다. 파일 전체 assertion 인자를 다시 확인했으며 비밀 문자열을 expected/actual로 넘기는 경로는 남지 않았다. 입력은 Press/프레임/Release 및 QueueTextEvent 경로를 유지하고 입력 도달 검증도 유지한다.

최종 판정: **코드상 차단 문제 없음**. UI 테스트 재실행 가능한 상태이며 실제 합격 여부는 테스트 담당의 실행 결과로 판정한다. 제품/Editor/서비스 조작은 수행하지 않았다.

### GameView 문자 입력 도우미 재리뷰

대상 테스트 SHA256: `0FBF6838ACB8CEA0E5C27D3E2E5C22735E117768562114952A179B7C75140AC2`.
QueueTextEvent 도달 실패 후 Coordinator가 승인한 도우미 변경만 확인했다. TMP 클릭 뒤 열린 GameView에 `SendEvent(KeyDown/KeyUp)`를 프레임별로 공급하며 `.text` 대입·TMP handler·제품 Submit 직접 호출은 없다. 실제 입력 문자열 일치 bool assertion과 기존 서버 합격 조건 및 마스킹은 유지한다. 입력값은 도우미 내부 메모리에서만 사용하며 로그·CLI 인자·파일로 출력하지 않는다. **코드상 차단 문제 없음**. Editor 전용 경로이며 실제 문자 도달·UI 합격은 재실행으로 확인해야 한다. 리뷰어는 Editor/서비스를 조작하지 않았다.

### GameView 입력 큐 도우미 재리뷰

대상 테스트 SHA256: `7B649410302AE31885B5F7BCD6B1DADEDFE39981D4EB9EB567C38681C19D56C3`.
앞선 QueueTextEvent와 GameView.SendEvent 시도는 실제 TMP 입력 도달 검증에서 실패했으므로 UI 통과 근거가 아니다. 이번 변경은 승인된 `EditorGUIUtility.QueueGameViewInputEvent`로 KeyDown/KeyUp을 공급하며 같은 입력 도달 bool·서버 결과 조건을 유지한다. 처음 포커스한 EditorWindow는 테스트 finally에서 복구하며 runInBackground는 변경하지 않는다. 직접 필드 대입/handler 호출·비밀 출력 추가는 없다. **코드상 차단 문제 없음**. 격리 UI 1회 재실행 가능한 코드 상태이며 실제 입력 도달 여부는 여전히 실행으로 확인한다. 제품 변경과 리뷰어의 Editor 조작은 없다.
