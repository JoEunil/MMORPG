현재 클라이언트 파일 경로는 기능명 구조로 갱신했습니다. 아래 계약 분석·운영 기록은 작성 당시 원문이므로 현재 입력/실행 상태는 실제 코드와 HANDOFF를 확인합니다.

# C06 — 월드 채팅 계약과 구현 범위

2026-10-19, 서버 계약 담당. 이 문서는 소스 정적 분석이며 기능 테스트 결과가 아니다. 제품 코드·서비스·DB·Unity Editor는 변경하지 않았다.

## 결론과 작업 단위

기존 서버를 그대로 사용해 전체 채팅 입력·송수신·이름 표시를 연결할 수 있다. 서버 변경은 필요 없다. ClientCore의 구형 이름 포함 채팅 파서를 현재 profile ID 형식으로 맞추고 기존 24/25 조회를 연결한다. Unity는 기존 Canvas 왼쪽 아래 최근 12줄, TMP 입력란, 보내기만 추가한다. 귓속말/지역 선택/이름 변경/신규 채널은 이번 UI 범위에서 제외한다. 기존 이동·스킬 코드는 이미 focused TMP_InputField를 차단하므로 이 경계를 재사용한다.

목표: 실제 서버가 돌려준 자기 메시지와 다른 플레이어 메시지를 한 번씩 표시하고, 입력 중에는 이동·스킬이 나가지 않는다. 전송 버튼을 누른 것만으로 성공 줄을 만들지 않는다. 이름을 못 받아도 서버 chat ID와 본문을 표시한다.

## 조사 기준

- HEAD `1d5a3d69074a1de6bafde30f1ec4dfe5cac1a951`, 작업 브랜치 `codex/unity-ai-client`.
- R01의 미커밋 NonZoneHandler/StateManager/LobbyZone/worker 변경과 기존 ClientCore 미커밋 호환 변경이 있는 상태다. 이를 보존한다. 채팅 선언/ChatThreadPool/ProfileCache/NetPacketFilter에는 현재 해당 파일 diff가 없다. Packet.cs에는 기존 Full 호환 변경이 있다.
- HANDOFF의 최신 실행 서버는 R01 후보이며, 후보 파일 `UnityClient/Builds/R01-Candidate/bin/MainServer.exe`의 실제 SHA256은 `01C6261AB5C3448E2BA5F336CE70FB20E4DBA4B9ED47AD70002F22C88594D108`로 인계와 같다. 이번 역할은 서버 프로세스 재기동/실패킷 동작을 관찰하지 않았으므로 파일 해시 일치만으로 실행 소스 동일성을 새로 보증하지 않는다.
- 근거: `CoreLib/PacketTypes.h`, `Config.h`, `NonZoneHandler.cpp`, `ChatThreadPool.cpp/.h`, `PacketWriter.cpp`, `ZoneState_Snapshots.cpp`, `CacheLib/ProfileCache.cpp`, `NetLibrary/ClientContext.cpp`, `NetPacketFilter.cpp`, `ClientCore/PacketHelper/Packet.cs`, `Network/Handler.cs`, `Services/NetworkService.cs`, `ViewModel.cs`, `UnityClient/Assets/Client/Runtime/World/WorldScreen.cs`, `CombatScreen.cs`.

## 실제 wire 계약

모두 packed 1, 현재 Windows x64 구현의 little endian이다. 공통 헤더 8B: magic uint16 `0xABCD`, 전체 길이 uint32, opcode uint8, flags uint8. 아래 채팅/프로필 패킷은 flags 0이다. `CHAT_REQ/RES`라는 enum은 없고 실제 opcode는 다음과 같다.

| 방향/opcode | 본문과 길이 |
| --- | --- |
| C→S CHAT 13 | scope u8, messageLength u16, targetChatID u64, raw message. 전체 `19 + messageLength`B. scope Global=0/Zone=1/Whisper=2. C06은 Global=0, target=0만 송신 |
| S→C CHAT_BROADCAST 15 | chatCnt u16, totalMessageLength u16, scope u8, **항상 고정 10개** ChatEntity(각16B), 이어서 raw message 영역. 전체 `173 + totalMessageLength`B |
| ChatEntity | senderChatID u64, offset u16, messageLength u16, senderProfileId u32. offset은 전체 패킷이 아닌 raw message 영역 시작 기준 |
| S→C CHAT_WHISPER 14 | messageLen u16, senderChatID u64, senderProfileId u32, raw message. 전체 `22 + messageLen`B. 이번 UI에서는 송신하지 않지만 기존 수신 파서는 형식에 맞춰 유지 |
| C→S PROFILE_BATCH 24 | count u16 + count개의 profileId u32. count 1~50, 전체 `10 + 4*count`B. 선언 배열50개를 모두 보낼 필요 없음 |
| S→C PROFILE_BATCH_RES 25 | count u16 + count개의 ProfileEntry. count 0~50, 전체 `10 + 40*count`B. **이 응답은 고정50개가 아니다** |
| ProfileEntry | profileId u32, version u32, name[32] raw bytes. UTF8 이름은 서버가 최대31B 복사하고 나머지를0으로 채움. NUL까지 decode |

채팅 본문은 길이가 명시된 `std::string` 복사다. 서버는 NUL 종결을 요구하지 않는다. 기존 ClientCore는 `message+'\0'`를 보내지만 새 송신은 NUL 없이 raw UTF8을 보내는 것이 계약과 맞는다. 크기는 C# 문자 수가 아닌 UTF8 **바이트 수**로 검사한다. `MAX_MESSAGE_LEN=128`은 선언만 있고 현재 수신 처리에서 그 한도를 적용하는 코드는 확인되지 않았다. C06 클라이언트 정책은 UTF8 1~128B, 공백만/줄바꿈/내장NUL 거부, 초과는 명시 오류로 하고 UTF8 중간 절단을 하지 않는다. 이 정책을 서버가 강제하는 검증이라고 설명하면 안 된다. 수신은 로컬 송신128B 제한을 강요하지 않고 실제 프레임/각 메시지 경계를 검사한다.

수신 검증: magic/전체 길이/flags와 opcode별 최소길이, chatCnt<=10, profile count<=50, 메시지 payload 길이와 totalMessageLength 일치, 각 offset+length가 payload 안에 있는지 확인한다. count를 검증하기 전에 배열 할당/접근하지 않는다. profile 응답은 count0도 정상이며 잘못된/중복ID·낮은 version으로 새 캐시를 덮지 않는다. UTF8 본문과 이름은 TMP rich text로 해석하지 않는다.

## 실행 경로·권한·에코·실패

`TCPSocket → NetPacketFilter(존재·인증·cheat mask) → PacketDispatcher(flags0) → NonZoneHandler::Chat → ChatThreadPool → PacketWriter → IOCP` 경로가 실제로 있다. ChatThreadPool은 서버 session→chatID/profileId 매핑을 사용한다. 발신자가 자기 ID/이름을 패킷에 넣어 위조하는 구조가 아니다. 해당 매핑은 최초 월드 진입의 AddChatSession에서 만든다. C06은 성공한 월드 진입 상태에서만 송신을 활성화한다.

Global은 모든 등록 chat session으로 보내므로 자기 메시지도 서버 broadcast로 돌아온다. 로컬 에코를 추가하면 두 번 표시된다. 서버는 최대10개의 메시지를 하나의 패킷에 합치며 한 패킷에 같은 발신자의 여러 메시지도 가능하다. 메시지별 서버 message ID나 ACK는 없다. 보내기 성공은 소켓 전송 완료이며 전달 성공은 실제 echo 관찰로만 말할 수 있다. 서버 큐 포화·미등록 chat session·프로필 누락에 대해 개별 실패 응답은 없다. echo가 없으면 성공을 꾸미거나 자동 재전송하지 않는다. 전송 예외는 입력한 내용을 보존하고 짧은 실패 상태로 표시한다.

chatID는 공개용 채팅 식별자이며 characterID/sessionID/zoneInternalID와 다르다. 본문 없이도 이름 대체 표시는 `플레이어 #<chatID>`처럼 **서버가 보낸 chatID**로 한다. 가짜 이름을 생성하지 않는다. 자기 chatID는 ZONE_CHANGE_RESPONSE 17의 chatID이며, snapshot의 profileId와 혼동하지 않는다.

PROFILE_BATCH 24도 동일 인증 경로를 통과한다. 별도 Zone/특정 캐릭터 소유권 제한 없이 서버의 메모리 profile 캐시에서 요청 ID를 조회한다. 임의 오프라인 DB 조회가 아니다. 캐시는 캐릭터 load 때 채워지고 disconnect 때 unload된다. 없는 profile은 응답에서 생략한다. **25 자동 push는 없으며 24를 보내야 한다.** 이름 조회 실패가 채팅 수신/표시 실패가 되어서는 안 된다.

정적 관찰 위험: NonZoneHandler::Chat의 `ChatEvent event;`는 Global 경로에서 `key.id`를 초기화하지 않는다. 그룹 키 해시가 이 값을 읽는다. 이는 기존 서버 결함 후보이며 C06 최소 클라이언트 작업에 몰래 섞지 않는다. 현 서버 무변경 배치에서 실제 echo/상대 전달 실패가 나타나면 근거를 남겨 정확한 최소 서버 후보(`ChatEvent event{};`)를 별도 분석한다. 이번 문서는 이를 적용하지 않았으며 안정성을 보증하지 않는다. 서버는 opcode별 짧은 본문/메시지 한도 강화도 충분하지 않으므로 정상 C06 송신만 검증하고 공격성 부하/새 보안 정책은 추가하지 않는다.

## ClientCore 최소 변경 제안

1. `Packet.cs`: 14/15의 name[32]를 senderProfileId u32로 정정하고 Message에 profileId를 전달한다. 24/25 enum과 ProfileEntry, 길이 검증 파서/요청 직렬화를 추가한다. 요청 경로는 기존 PacketBuilder/NetworkService/interfaces를 사용한다. 변경은 원본 ClientCore 한 곳에 유지하고 기존 빌드 방식으로 Unity에 연결한다.
2. `Handler.cs`: 25 정상 count0 포함 수신 dispatch. `ReceiveTick`은 simulation 전용이므로 프로필 state를 중복 저장하지 않는다.
3. `ViewModel`의 단일 profile 캐시에서 수신 profileId→(version,name)를 관리한다. 메인 스레드 dispatcher 안에서 캐시/채팅 이벤트를 순서대로 처리하면 새 별도 Manager나 중복 락 계층이 필요 없다. 수명 종료 뒤 dispatcher 콜백은 취소하며 재연결에는 이전 캐시/미처리조회가 섞이지 않게 한다.
4. unknown profile은 최대50개로 묶어 요청하고 in-flight 중복을 막는다. 응답 누락 ID는 같은 세션의 같은 관찰 버전에서 반복 요청하지 않는다. 이름은 없을 때 먼저 chatID로 표시하며 이미 표시한 줄의 이름 갱신을 위해 별도 이벤트/대기 큐를 추가할 필요는 없다. 이후 메시지에서 받은 이름을 쓴다. 실패·timeout으로 이미 표시한 채팅을 지우지 않는다. 무한 timeout/retry 루프를 추가하지 않는다. 개별 chat 자체에는 version이 없으므로 이미 알려진 이름을 매 메시지마다 다시 요청하지 않는다.
5. snapshot Full의 profileId/profileVersion 및 Delta field9(profileVersion)로 캐시 노후를 판단한다. zoneInternalID→profileId는 이미 들어오는 Full에서만 얻는다. 새 profileVersion 관찰 시 해당 ID의 누락 억제를 풀고 재조회하며, 늦은 이전 응답은 높은 버전 캐시를 덮지 않는다. 이름 변경 UI/서버 요청은 만들지 않는다. 현재 Full struct는 이미 profile ID/version 형식이지만 이를 이름 캐시에 연결하는 구현은 없다.
6. 기존 OnChatReceived의 scope/chatID/name/body 소비 경계를 유지하고 캐시에서 이름 또는 chatID fallback을 채운다. `_network.Chat`는 현재 예외를 내부 로그로 삼키므로 C06의 사용자 실패 표시에 필요한 최소 결과/오류 경로를 구현자가 정한다. 일반 송신 구조 전면 교체는 필요 없다.

## 수용·단일 배치 계획

구현과 독립 리뷰 후 동결한 동일 코드·에셋으로 한 번 실행한다. 중간에는 컴파일/정적 검사만 한다.

- 계약 fixture: 단일/복수 chat15 offset, 14 profile ID 형식, 25 count0/복수, UTF8 128B/129B·NUL, 잘린/초과 count·offset·profile version 역전, 누락 프로필 재요청 억제와 disconnect 콜백 정리를 확인한다.
- 실제 입력: 계정1 로그인/진입 후 ASCII와 한글/숫자·태그 유사 문자 메시지 입력·전송, 자기 서버 echo 한 번/이름 해결 확인. 가능하면 지정 계정2의 실제 별도 연결에서 교차 송수신도 같은 배치에 포함한다. 상대를 사용할 수 없다면 교차 수신은 미실행으로 남긴다.
- 입력 중 진행 중 이동 취소와 1/2/3 스킬 미송신, send 클릭의 월드 이동 차단, 입력 해제 뒤 정상 이동/스킬 복귀. UI만 보고 네트워크 미송신을 단정하지 말고 action 송신 또는 서버 결과와 대조한다.
- 공백/초과 입력 전송 거부, 연결 종료 후 발신 비활성·기존 줄 정리, 최대12줄 유지/본문 rich text 미해석, 캡처1장을 확인한다.
- 예상 비용: 계약 fixture와 기존 관련 회귀를 하나의 묶음으로 실행하고 실제 로그인 흐름1회(상대 연결 필요 시1회 추가). 무제한 반복/부하 테스트는 제외. 실패면 원인과 영향만 수정하고 영향 검증을 재실행한다. 모든 수용 항목의 통과/실패/미실행을 남기면 종료한다.

호환/복구: 서버 wire·DB·운영 정책 불변. 문제가 생기면 C06 ClientCore/Unity 변경만 이전 검증 버전으로 되돌린다. 원래 R01/C01~C05 미완료 변경을 함께 되돌리지 않는다. 중요한 ClientCore 후보의 GitHub PR/push는 최종 리뷰·배치 결과 준비 후 Coordinator의 사용자 승인 경계를 따른다.
