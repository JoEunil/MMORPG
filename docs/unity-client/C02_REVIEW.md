# C02 독립 코드 리뷰

2026-10-18 KST. 판정: **코드상 차단 문제 없음**. 정적 검토에서 수정이 필요한 C02 제품 결함은 발견하지 못했다. 코드 리뷰는 실제 UI/서버 진입 검증 통과를 대신하지 않는다.

## 고정 범위와 근거

- ClientCore 지정 6파일: PacketHelper/Packet.cs, Network/TCPSocket.cs, Network/ReceiveTick.cs, ViewModel.cs, Services/NetworkService.cs, Initializer.cs.
- Unity: Assets/C01/C01LoginScreen.cs, Assets/C01/Editor/C01SceneBuilder.cs의 C02 연결 변경, Assets/C02/ 런타임·Editor·Scene·Material·asmdef, Assets/Tests/C02/ fixture·asmdef. C01의 기존 AuthService/ViewData/Interfaces 등 미커밋 변경을 새 C02 변경으로 판정하지 않았다.
- 실제 서버 PacketTypes.h, PacketWriter.cpp/.h, BroadcastThreadPool.cpp와 C02_CONTRACT.md를 대조했다. 서버/DB/제품/테스트/Editor/프로세스 조작은 수행하지 않았고 이 리뷰 문서만 작성했다.
- BuildProvenance.txt의 제품·Scene·Material·asmdef·meta·원본/Unity DLL SHA256을 현재 파일과 대조해 모두 일치함을 확인했다. Release/Unity DLL은 동일 SHA256 `6803B74024DB198EF0619294E996DC4EEDF4E15A28F26D12433D89DD148D70AC`이다. 컴파일 성공은 구현 인계의 기록이며 리뷰어가 재빌드하지 않았다.
- 현재 tracked diff 목록에는 서버 소스 변경이 없다. C02-DB-01/r1과 C02-AUTH-01/r1은 승인 대기이며 이 리뷰가 실행 허가를 부여하지 않는다. Assets 밖 UnityScripts/Game/PlayerManager.cs·PlayerState.cs의 구형 Full.charName 소비 호환 문제는 알려진 후속 포팅 항목이며 현재 UnityClient 빌드 밖이다.

## 확인한 동작

- `TCPSocket.cs:69` 부근에서 Enter opcode6=76/flags0, Zone opcode17=31/flags1, Full/Delta 및 자동 Monster/Action의 최소 길이·flags를 검사한다. 프레임 magic/8바이트 하한/1MiB 상한과 완전한 프레임 수신 뒤 전달을 유지한다. 고정 패킷의 잘린 body가 unmanaged read로 전달되지 않는다.
- `Packet.cs:258` Full은 pack1·43바이트이며 internalID/profileId/profileVersion/hp 이하 오프셋이 서버와 같다. Delta는 10바이트다. `Packet.cs:581`의 가변 파서는 실제 frame 길이와 header.length, `10 + count × sizeof(T)`를 long 산술로 검사한 뒤 할당·읽기한다. MonsterFull/Delta/Action 기존 필드 형식은 유지한다.
- `ReceiveTick.cs:84`의 예외 경계가 가변 파싱 실패를 잡고 OnFailed로 전달한다. Initializer의 구독을 통해 ViewModel.Error→메인 스레드 OnConnectionFailed→C01 ReleaseClient/Dispose로 socket/tick·이벤트를 정리한다. 정지 시 현재 thread에 Join하지 않는다. 관련 subscriber와 dispatcher 구현을 포함한 C02 경로에서 처리되지 않은 수신 thread 예외를 발견하지 못했다.
- ViewModel Enter/Zone 요청은 송신 실패를 UI 오류 경계로 전달하고 pending 상태로 중복 요청을 막는다. Enter 성공은 자동 Zone ENTER를 한 번 요청하며 UI가 중복 송신하지 않는다. Enter/Zone/Full/Delta dispatcher callback은 dispose 이후 반영되지 않는다. Zone 실패도 dispatcher에서 전달한다.
- C02 선택 UI는 현재 서버 목록의 행만 선택하고 송신 직전에 선택 ID의 목록 포함을 재확인한다. 영속 characterID와 Zone internalID를 별도 표시한다. 내부 ID가 일치하는 Full과 Zone 응답을 모두 받아야 월드를 표시하며 Full→Zone 및 Zone→Full 순서를 처리한다. AOI Full에 내 ID가 없다는 이유로 삭제하지 않는다.
- 서버 `(x,y)`는 캡슐의 Unity `(x,0,y)`로 반영하고 유한 좌표를 확인한다. Enter 대기부터 Zone/Full 확인까지 기존 15초 busy deadline을 사용한다. 오류·타임아웃·연결 종료·클라이언트 교체 시 월드와 pending Full을 비우고 구독을 해제한다. C01 로그인/빈 목록의 status=0 의미는 유지한다.
- 현재 완료 조건에 필요한 최소 pending Full 저장·단계 상태·수명 연결로 구성됐으며 삭제를 요구할 불필요한 범용 계층이나 중복 상태는 발견하지 못했다.

## fixture 검토와 실행 한계

- Full fixture는 서버의 43바이트 전체 필드/오프셋과 잘림·여분 바이트·count 불일치·최대 count 거부를 확인한다. state/dispatcher fixture는 Enter 실패/Zone 실패, Dispose 뒤 callback 취소, Zone/Full 양쪽 순서와 일치 ID·좌표만 표시하는 조건을 확인한다.
- state fixture의 reflection/직접 handler 호출은 주석과 클래스 이름으로 state fixture임을 명시한다. 실제 UI 입력이나 실서버 연결 통과로 주장하지 않는다. 테스트 데이터는 합성 값이며 비밀번호·토큰을 assertion actual/expected에 넣는 경로는 없다.
- 최초 실행에서 보고된 NUnit Int32→UInt16 TestCase 매개변수 변환 실패에 대한 최종 수정은 `int count` 수신 후 fixture 생성 경계에서 `checked((ushort)count)` 변환이다. TestCase 값과 `Assert.Throws<ArgumentException>` 조건은 유지됐으며 기대 완화가 아니다. 아래 최종 소스를 검토했다.

| 파일 | 최종 SHA256 |
| --- | --- |
| C02EntryStateFixtureTests.cs | `1F58894F41341B7EDB0EE917717E9DA88398017F56E7F15308B9A7876885D0CA` |
| C02FullPacketFixtureTests.cs | `84F481AEAADF57FC7034E3629BEA8A3874B29C8600ABCC7DC796E21D9FB04902` |
| C02.PlayModeTests.asmdef | `63A54B80A308D5B183940924E9B3AADA898B57100F689A6EB37BDDC16B8C4B22` |

미검증: 리뷰어는 fixture를 실행하지 않았다. 최종 fixture 합격은 테스트 담당의 고정 hash 실행 결과를 따른다. TCP malformed 길이/flags와 ReceiveTick 오류→실제 Dispose의 실행 재현, 송신 실패·타임아웃·연결 종료·구독 중복의 실제 UI 관찰, 실제 목록→선택→Enter→Zone→자기 Full·좌표, Play 종료/재시작, 캡처는 이 정적 검토의 통과 항목이 아니다. C01의 과거 실제 로그인/빈 목록 통과 기록은 C02 재실행을 대신하지 않는다. 실행 서버 바이너리와 서버 소스의 빌드 일치도 계속 미검증이다. 임의/타 계정 ID 실서버 시험은 수행하지 않았다.

제품 수정 시 해당 경로와 DLL/provenance만 다시 확인하고, 테스트만 수정되면 그 delta와 실행 기준 hash를 확인한다.

## 빈 목록 UI 도우미·공유 runner 재리뷰

2026-10-18 KST. 판정: **코드상 차단 문제 없음**. 제품은 변경하지 않았고 아래 두 파일의 최종 소스만 추가 검토했다.

| 파일 | 최종 SHA256 |
| --- | --- |
| Assets/Tests/C01/Editor/C01EditorInputRun.cs | `8A78B02D92AB0818A8DCCE985A8CD4F65C835C01D04D112A557891C5EEE8FE15` |
| Assets/Tests/C02/C02EmptyListUiTests.cs | `502A035AA866C45A82EFEF2D17F5240E4B9283E6F972A5ABD7F7BB99E70BE596` |

- 공유 runner의 기존 인자 없는 Start는 기존 C01 coroutine/assertion을 실행한다. 새 IEnumerator/outputPath/passMessage 오버로드는 실제 C01/C02 두 소비자가 사용하며, 단일 실행·Play 조건·120초 상한·지원 yield 제한과 Editor 이벤트 해제·iterator Dispose를 유지한다. 결과는 Not a NUnitRunner result로 표시하고 예외 메시지·스택을 결과 파일에 복사하지 않는다.
- C02 도우미는 기존 Windows 사용자 계정 loader를 reflection으로 재사용한다. 실제 MouseState 누름/뗌과 GameView 문자 이벤트로 로그인 UI를 입력하고 focus·문자 도달을 bool로 검사한다. 로그인/목록 검증 구간에 제품 Submit/handler 호출·입력값 직접 대입은 없다. authenticated, status=0/count=0, 단일 목록 응답, 10개 행 비활성, Enter 불가, world hidden 조건을 유지한다. NUnit/SceneManagement namespace 추가는 컴파일 수정이며 합격 조건을 바꾸지 않는다.
- 비밀 문자열을 assertion actual/expected나 새 로그·결과 파일에 넣지 않는다. 비밀번호 필드 비움과 상태 문자열의 자격 증명 포함 여부를 bool로 검사한 후 빈 목록 PNG를 기록한다. finally에서 비밀번호 입력·계정 객체를 비우고 임시 Mouse·runInBackground·이전 포커스를 복구한다.
- 뒤따르는 쿼터뷰 캡처는 Stage/선택 ID/Full/Zone을 직접 주입하는 별도 fixture preview다. PNG에는 `FIXTURE PREVIEW — NOT A SERVER ENTRY`를 표시하며 실제 UI/서버 입장 통과 근거가 아니다. finally에서 기존 모델에 재연결해 fixture 월드를 숨기고, HasOwnFull=false·EnterRequestCount=0을 확인한다. PNG 삭제는 private 함수의 두 고정 C02 파일명 호출에 한정되며 기존 파일이 있을 때만 수행한다.

실행 상태: 총괄은 기존 최종 C02 fixture 10/10 통과를 실제 CLI test_status completed로 확인했다고 전달했다. 리뷰어가 재실행한 결과는 아니다. 새 빈 목록 UI 도우미와 fixture preview 캡처는 이 재리뷰 시점에 미실행이며, 실제 입력 도달·PNG 내용·실행 후 정리는 별도 관찰이 필요하다. 승인 대기 중인 실제 캐릭터/Zone 입장과 구분한다. 제품·Editor·서비스 조작은 수행하지 않았다.

### 입력 실패 진단 delta

대상: C02EmptyListUiTests.cs SHA256 `F144B08F684B2604FBA607C7D03E9602EC85A379844734871745BED106965027`. 판정: **코드상 차단 문제 없음**.

TypeText의 focus/문자 도달 실패에만 추가 진단을 기록한다. 내용은 고정 UI 객체명, 문자 수, focus/선택 일치, 입력 action·장치 유무, 화면 크기·좌표다. 입력 문자열·비밀번호·토큰·예외 원문은 출력하지 않는다. 입력 focus/prefix/최종값 일치의 bool assertion과 입력 이벤트·기존 서버 결과 조건·finally 정리는 유지한다. 초기 UI 실행 실패는 실패 기록이며 이 진단 추가가 통과 근거는 아니다. 제품·Editor 조작 없이 정적 delta만 확인했다.

### Scene teardown 수정 재리뷰 — 지적과 해결 기록

총괄의 실제 UI 실행에서 기존 C02 OnDisable→ChangeClient(null)이 이미 파괴된 Button.gameObject를 읽어 MissingReferenceException을 일으키고 ErrorPause로 입력이 정지하는 결함이 확인됐다. 앞선 정적 리뷰는 이 파괴 순서 결함을 놓쳤으며 실제 UI 통과로 볼 수 없다.

당시 재리뷰 기준은 C02WorldScreen.cs SHA256 `E269BFBCFA15C8C60A83A69CC2DC7BF5F55975F4544F0D6E16EEA4120F4EF6BE`, C01LoginScreen.cs `B8DB07E59563FF3E1300415FCEDD950E0B6DF21DD86C9EB4A8C2EA673F57BC93`이며 provenance 두 항목과 일치했다. DetachModel과 destroyed UI null guard, C01 ReleaseClient의 public 전환은 확인했다.

당시 판정: **차단 문제 있음 — P2 비활성화 뒤 월드 표시 잔존**. E269의 C02WorldScreen.OnDisable은 managed 상태만 비우며 월드/HUD를 숨기지 않는다. C02Client 또는 C02WorldScreen만 비활성화하면 별도 hierarchy의 C02World/WorldHud는 계속 active여서 연결 종료 뒤에도 이전 SERVER-CONFIRMED 도형/HUD가 보인다. 최소 수정은 OnDisable에서 Unity-null 보호된 ResetWorld를 호출하거나 worldRoot/worldHud에 같은 보호를 둔 SetActive(false)를 수행하는 것이다. 전체 Scene 파괴 시험만으로는 이 경로를 확인할 수 없다. 총괄에 전달했으며 리뷰어는 제품을 수정하지 않았다.

최종 수정 확인: C02WorldScreen.cs SHA256 `3226DD3179F277FE5525E4A256425D6FD340F5F6592F1382F18ED5B7DD444A8A`, C01LoginScreen.cs는 위 B8DB07 기준 그대로다. BuildProvenance.txt SHA256 `4CB5FDD17F622E91FB1A0FA83413F436C8596E16519AA0840A161CC832867921`의 두 항목이 현재 파일과 일치한다. OnDisable에 Unity-null을 확인한 worldRoot/worldHud.SetActive(false) 두 줄을 추가했다. 파괴된 UI를 다시 활성화하지 않고 월드/HUD를 숨기며 DetachModel·managed 상태 정리·ReleaseClient를 유지한다. ChangeClient의 destroyed UI guard도 유지돼 C01 ClientChanged에 의한 teardown 호출을 보호한다. **지적한 P2는 코드에서 해결됐으며 이 수명 delta에는 차단 문제 없음**. 실제 reload가 ErrorPause 없이 끝나는지와 live component disable 뒤 두 객체가 숨겨지는지는 총괄의 실행 검증 대상이다.

실행 구분: 총괄은 이전 E269에서 실제 로그인/빈 목록 UI와 fixture 상태 assertion 통과, 빈 목록 PNG 시각 확인을 전달했다. fixture 필드 캡처가 캡슐 없이 cyan 단색으로 보이는 별도 시각 문제는 조사 중이며 이 수명 수정 리뷰로 해결됐다고 판단하지 않는다. 실제 서버 캐릭터 입장은 여전히 미검증이다.

### 캡처 준비 대기 delta와 수명 실행 근거

대상: C02EmptyListUiTests.cs SHA256 `4012FED859890E44626DD899CF751DE527C33835B4EBD547E7521CC6A7DF86AB`. 판정: **코드상 차단 문제 없음**. fixture preview의 기존 상태/좌표 assertion 뒤 두 프레임을 기다리고, ShaderUtil.anythingCompiling이 false가 될 때까지 최대 10초 대기한 후 false assertion과 추가 한 프레임을 거친다. 기존 라벨·실제 로그인 입력/빈 목록 합격 조건·fixture reset·비밀 없는 assertion을 유지한다. 준비가 끝나지 않으면 실패하며 shader 관련 제품 설정이나 렌더링 코드는 변경하지 않는다. 최종 helper의 캡처 실행/시각 결과는 총괄이 확인할 항목이다.

총괄은 최종 제품 3226 기준 실제 빈 목록 UI PASS와 Scene reload 시 ErrorPause 없음, 여러 프레임 유지한 합성 상태에서 캡슐/평면이 보임을 전달했다. 리뷰어가 읽은 `Captures/c02-component-disable.txt`도 합성 상태임을 명시하며 before=true, worldHidden/hudHidden/fullCleared/modelReleased=true, paused=false를 기록한다. 따라서 앞서 지적한 component disable 수명 문제는 코드 수정과 해당 실행 기록 모두에서 해결 근거가 있다. 이 합성 상태 시험과 시각 관찰은 실제 서버 캐릭터 입장 통과를 뜻하지 않는다. 리뷰어는 제품·Editor를 조작하지 않았다.

### 실제 선택·진입 UI 도우미 최종 재리뷰

대상: C02EmptyListUiTests.cs SHA256 `0BC34D8A7F0C9DC728A9CD7A710EE314BE5BB55C78A583B7213CE5E0EE0C3A13`. 판정: **코드상 차단 문제 없음**. 이번 범위는 새 실제 진입 시나리오와 입력 프레임 대기·Zone assertion·비밀 없는 진단이며 제품/서버 변경은 재검토하지 않았다.

- 실제 진입 시나리오는 MouseState와 GameView 문자 이벤트로 로그인·서버 반환 행 선택·Enter 버튼 입력을 수행한다. 목록의 승인 이름/expectedCharacterID·status1/count1·단일 응답, 실제 선택 ID와 Enter 활성, EnterRequestCount=1을 확인한다. 실제 경로에서 제품 Submit/handler/상태 setter를 직접 호출하지 않는다. reflection은 선택 버튼 배열을 읽는 데 사용하며 직접 callback 주입은 별도 fixture preview 경로에만 남아 있다.
- live model의 Zone/Full 이벤트를 별도로 관찰해 Zone=1·nonzero internalID·같은 ID의 Full과 화면 내부 ID/Zone/좌표·캡슐 `(x,0,y)`·HUD 문자열 일치를 요구한다. `receivedZone == 1` 추가는 기대 조건을 강화한다. 실제 성공을 합성 좌표/상태로 대체하지 않는다.
- Click의 중립/누름/뗌 이벤트 사이마다 NextPlayerFrame을 기다린다. QueuePlayerLoopUpdate 뒤 Time.frameCount 증가를 최대 3초 안에 요구하므로 Editor-update만 돌고 Player 입력 프레임이 지나지 않은 상황을 통과시키지 않는다. 기존 focus·문자 prefix/최종값·서버 결과 assertion은 유지한다.
- 추가 CLICK 진단은 고정 대상명·frame·선택 객체명·mouseAtTarget bool만 기록한다. 입력 문자열·비밀번호·토큰은 없다. mouseAtTarget은 Editor에서 읽은 장치 상태 진단이며 성공 assertion이 아니다. 비밀 포함 여부 assertion은 bool로 유지한다. observer 해제·비밀번호/계정 참조 비움·임시 Mouse 제거·runInBackground/포커스 복구를 finally에서 수행하고, 녹화용 연결 유지는 주석으로 명시한다.

실행 구분: 총괄은 승인 DB 준비 후 이 실제 UI 시나리오를 두 번 통과했고 실제 Play 종료/재시작도 확인했다고 전달했다. 선택 characterID=5011, EnterCount=1, Zone1, 위치(10,10), 접속 간 새 internalID를 관찰한 실행 근거는 총괄 결과를 따른다. 리뷰어가 별도로 UI/DB/서버를 실행한 것은 아니다. Recorder의 37개 동일 프레임으로 만든 6초 GIF는 실제 진입 완료 상태 캡처이며 전환 영상으로 표시하면 안 된다. 이 helper 리뷰는 별도 AUTH 변경의 현재 서버 배포/소유권 차단 검증을 뜻하지 않는다.
