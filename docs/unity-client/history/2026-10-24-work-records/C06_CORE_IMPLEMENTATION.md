# C06 ClientCore 채팅 호환 구현

2026-10-19. 구현 담당. 이 문서는 저작·컴파일 결과이며 기능 테스트 통과 기록이 아니다.

작업 카드: 구형 name[32] 채팅 형식을 현재 서버 profile ID wire로 맞추고, 기존 dispatcher/ViewModel에서 실제 이름을 조회한다. 최소 위치는 Packet/PacketBuilder, Handler/TCPSocket, NetworkService/Interfaces, ViewModel이다. 검증은 아래 wire/profile fixture와 Coordinator의 동결 후 단일 실제 입력 배치다.

변경:
- opcode14는 22B+raw, 15는 173B+raw와 고정10개16B entity로 파싱한다. profile ID를 Message에 전달하며 본문 NUL을 임의 제거하지 않는다. magic/길이/flags/scope/count/offset/엄격 UTF8을 확인하고 잘못된 C06 프레임 예외는 기존 socket disconnect 경계로 올린다.
- opcode24는 1..50 ID의 가변 요청, 25는 0..50개의 40B entry다. ID0/중복/잘린프레임/잘못된 UTF8/종결 없는32B 이름을 거부한다. 서버/DB wire 변경은 없다.
- ViewModel 한 캐시가 Full profileId/profileVersion 및 delta field9를 관찰한다. 단일 최대50개 요청 중복을 막고, 실제 채팅은 즉시 실제 server chatID fallback으로 전달한다. 낮은 version은 이름을 덮지 않는다. 기존 OnChatReceived signature를 유지했다.
- 요청ID 없는25의 늦은/빈응답이 새 요청을 해제하지 않도록 실제 pending ID와 현재 관찰 version 이상인 응답만 반영·해제한다. 미응답/생략 ID는 기존 dispatcher의 다음 Full/Delta/Chat/응답 진입에서 monotonic5초deadline을 확인해 그 요청관찰version에만 누락억제한다. 새 timer/thread/자동반복재시도는 없다. 새로운 profile version을 관찰하면 다시 요청할 수 있다.
- 송신은 엄격 UTF8 1..128B, 공백만/CR/LF/NUL/잘못된 surrogate 거부, NUL없이 raw로 보낸다. 기존 Chat void API에서 유효성/송신 예외를 UI로 전파하므로 UI가 입력을 보존할 수 있다. 월드입장 전·disconnect/dispose 이후 송신을 거부한다. 캐시 callback은 disconnect/dispose 뒤 전달하지 않는다. UI는 Global만 노출한다.
- INetworkService.Profiles(uint[]) 및 IViewModelNet.ProfilesReceived(ProfileEntry[])만 추가했다. 기존 C02EntryStateFixtureTests/C04ObservationTests/C05CombatTests의 fake는 Profiles noop 각1줄만 추가했으며 기존 assertion은 수정하지 않았다. ViewData는 변경하지 않았다.

컴파일:
- ClientCore `dotnet build ClientCore/ClientCore.csproj --no-restore -v minimal`: 오류0, NU1900 네트워크 취약성메타 경고1.
- C06Peer restore(ignore-failed-sources)·build --no-restore: 오류0, 같은 NU1900 경고2. 최초 no-restore의 assets파일 없음 실패는 restore로 해소했다.
- 새 C06ChatPacketTests는 실제 프로젝트의 NUnit DLL과 본체 DLL을 참조하는 obj 내부 임시 compile-only project/net9/C#9로 컴파일: 오류0/경고0. 최초 OS temp경로 compile은 NuGet access denied였으며 권한확대 없이 소유한 C06Peer/obj 안 compile-only로 수행했다. Unity 컴파일/테스트 실행을 대신하는 결과가 아니다.
- DLL을 Unity Assets에 복사하지 않았다. build-ClientCore.ps1, Editor/Play, 서비스/DB/계정/peer/기능fixture 실행 모두0. 실제 test-accounts.xml은 LoginId/SecureString 필드명만 읽어 형식을 확인했고 암호문/평문 값 출력·복호화는0.

동결 후 테스트 실행 지점:
- `C02.Tests.C06ChatPacketTests.RunPureChecks()`는10개 wire/profile fixture를 한 번 실행한다(정상multi-entity offset·whisper·가변profile/count0·잘린프레임/count/offset/flags/magic/UTF8·128/129B/NUL·fallback/누락억제·version역전·50개묶음·수명·timeout/lateempty). 현재는 전부 미실행이다.
- peer: `dotnet UnityClient/Tools/C06Peer/bin/Debug/net9.0/C06Peer.dll <runId> <readyPath> <resultPath> [timeoutSeconds]`. runId는 ASCII 영숫자/underscore/hyphen 1..48자, 기본180초/허용30..300초. ready/result는 txt key=value이며 runId/실제chatId/zone/internalId/횟수/결과만 기록한다. 자격증명 인자를 받지 않는다.
- peer는 기존 C04의 DPAPI CLIXML 경로·전용계정2 선택을 재사용한다. 샌드박스 LocalAppData 경로에 파일이 없으면 실제 UserProfile/AppData/Local 경로를 사용한다. 고정 Windows CryptUnprotectData/LocalFree만 사용해 메모리에서 읽고 암호문·임시plain byte[]/native메모리를 지운다. child secret stdout 경로가 없으며 로그는 비밀을 포함하지 않는다.
- 실제 account2 로그인/목록의 유일캐릭터5012 확인/진입·ownFull 뒤 ready, 상대가 보낸 정확한 `C06 <runId> 123 <b>literal</b>` 서버 Global 메시지를 받으면 `C06 peer <runId>` 한개만 송신한다. 상대1회·자기serverecho1회를 확인한 뒤 상대의 profile조회를 위해3초 유지하고 정상dispose한다. 예외문자열/본문/이름은 stdout에 쓰지 않는다. 정상실행은 Coordinator만 수행한다.

남은 위험:
- 기존 서버 NonZoneHandler::Chat의 Global ChatEvent key 미초기화 후보는 수정하지 않았다. 실제echo/상대전달 실패는 별도 원인분석 대상이다.
- 자동25push/requestID는 없다. dispatcher traffic이 없으면 이름조회deadline 정리도 다음traffic까지 미뤄지지만 채팅fallback은 즉시 표시한다. 늦은 누락/낮은version은 timeout 뒤 해당관찰version의 재요청을 억제한다.
- 이 ClientCore ViewModel은 기존 Initializer/Dispose의 세션수명을 따른다. 재접속은 기존 UI처럼 새 Initializer를 사용한다. 종료된 ViewModel을 같은 인스턴스로 재로그인하는 새 흐름은 추가하지 않았다.
- 서버 wire/서비스/DB, UI/Scene, Git 쓰기/게시를 수행하지 않았다. 실제 기능·입력·peer 검증과 독립 리뷰는 Coordinator 배치에서 확인해야 한다.
B8791E67C278430B9567C19013AC2417CF474129C55A6181FBBFDA6AFB049E52  ClientCore/PacketHelper/Packet.cs
BDB2DA5CC57DC0FBEB90D708F62E50F176029FBDF860C89D2CAA0F681FD751B1  ClientCore/PacketHelper/PacketBuilder.cs
044C3792DB24DFE8B99B0D4A5AD59F00DCC37920ABAD77509AFBCCFB3521DEE6  ClientCore/Network/TCPSocket.cs
3C8CC1C845B2B5C59B3F56E6F82C5F8E6513CB1513FFC47F11AD1F265C6DDFC6  ClientCore/Network/Handler.cs
7F732AC8CF9CE3202326CAD9A737D1CB84596E9A71A4D1E83805DC79E7DD8CDE  ClientCore/Services/NetworkService.cs
601473A888EE8A4C3410B8DB0CE838892581AAFDDCB623716325F55DFAFA6556  ClientCore/ViewModel.cs
B9AD4C85D7D858DB0C115B24A10F87EC143F262B7D10E876E55639273FDE9CAA  ClientCore/Interfaces.cs
74623EC214209DC7126E74DFBDD2A2A00231BE0BADDC237CE77A404429D1FEE6  UnityClient/Assets/Tests/C02/C06ChatPacketTests.cs
6C51D4B37243ABFD8F72D32C06E976B5BF4F7F5BF68FCCDA8372446317ECC9FD  UnityClient/Assets/Tests/C02/C06ChatPacketTests.cs.meta
957CFF33097E5F56E13AFB81E067BB6EC660CEB1ADF78F47090B5074A615F178  UnityClient/Tools/C06Peer/Program.cs
2EA407EFF3C8096DCE77F027FD30326DB551C1EB1A8BAFBEAA146D872188BF3B  UnityClient/Tools/C06Peer/C06Peer.csproj
