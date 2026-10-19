# C01 서버·ClientCore 계약

조사일: 2026-10-18 KST. 범위는 HTTP 로그인 → 게임 세션 발급 → TCP 인증 → 캐릭터 목록 응답이다. 서버·ClientCore·설정·DB 수정, 빌드, 실행은 이 조사에서 수행하지 않았다.

## 기준과 확인 한계

- 체크아웃 HEAD: `1d5a3d69074a1de6bafde30f1ec4dfe5cac1a951` (`codex/unity-ai-client`). 조사 시 `ClientCore`, `CoreLib`, `CacheLib`, `ExternalLib`, `Login`의 추적 파일 diff는 없었다. 기존 준비 파일은 보존했다.
- 준비 환경의 실행 후보는 원본 Game의 `x64/Release/MainServer.exe`다. 현재 파일 SHA256은 `529D16A75BD84417ADCFC4DD632830758D6F2939362B367FF0170B41511A9BD6`로 준비 기록과 일치한다. 이 바이너리와 조사 소스의 빌드 일치는 미검증이다. 원본 Game/Login 런타임 소스와 체크아웃 소스 일치도 본 조사에서 검증하지 않았다.
- 아래 계약은 소스에서 확인했다. 실제 응답·프로세스 복구·Unity 동작 통과는 Coordinator의 별도 실행 검증으로 확정한다.
- 환경 복구 후 Coordinator 대조: 실행 Login의 `routes/auth.js`는 체크아웃과 SHA256이 같다. `app.js`·`package.json`은 raw hash는 다르지만 `git diff --no-index`의 정규화 비교에서 내용 차이가 없다(CRLF 경고만). `Login/bin/www`는 체크아웃에 없으며 승인된 원본 실행 경로를 사용했다. 게임 서버 바이너리의 소스/빌드 일치는 계속 미검증이다.

## 요청·응답 경로

1. `ClientCore/Services/AuthService.cs:LoginAsync` → `POST http://127.0.0.1:3000/auth/login`, JSON `{login_id,password}`. `Login/routes/auth.js`는 bcrypt 비교 후 `{success:true,UserID,AccessToken}`를 반환한다. AccessToken은 JWT이며 소스상 유효기간은 3시간이다. 없는 계정·잘못된 비밀번호는 HTTP 400, DB 오류는 500이다.
2. 같은 서비스의 `GetSessionAsync` → `POST /auth/getSession`, `Authorization: Bearer <AccessToken>`. 인증 middleware는 누락·만료/무효 토큰에 401을 반환한다. 응답은 `{userId,sessionToken,gameServer:{ip,port}}`, 소스상 주소는 `127.0.0.1:9999`다. Newtonsoft.Json의 기본 대소문자 무시 매핑으로 기존 DTO에 대응한다.
3. getSession은 UUID 문자열을 Redis의 userId 키에 EX 600으로 저장한다. 같은 계정에서 다시 발급하면 이전 값을 덮어쓴다. Redis 오류는 500이다. 세션 토큰을 파일·로그·캡처에 기록하지 않는다.
4. `ViewModel.Connect` → `NetworkService.Connect` → `TCPSocket.Connect` 후 `PacketBuilder.CreateAuthPacket`. `CoreLib/NonZoneHandler.cpp:CheckSession` → `ExternalLib/SessionAuth.cpp`의 Redis GET 비교 → `ResponseSession` → `PacketWriter.WriteAuthResponse`로 응답한다. 실패=0, 성공=1이다. 소스상 인증 비교는 Redis 값을 읽으며 토큰을 소비/삭제하지 않는다.
5. `Handler.HandleAuthRes` → `ViewModel.AuthReceived`: 성공이면 `NetworkService.CharacterList`로 헤더만 송신한다. 서버 `NonZoneHandler.GetCharacterList`는 세션에서 userID를 얻고 channelID=1로 내부 메시지를 보낸다. `CacheLib/Handler.cpp:CharacterListRequest`의 DB 조회 결과 → `CoreLib/MessageQueueHandler.cpp:CharacterListResponse` → `PacketWriter.WriteCharacterListResponse`로 응답한다.
6. `ClientCore/Network/Handler.cs` → `ViewModel.ChatacterListReceived` → `ViewData.SetCharList`, 이후 `IMainThreadDispatcher.Post`로 `OnCharacterListReceived`를 발생시킨다. UI는 dispatcher를 Unity 메인 스레드에서 처리하고 `IViewDataUI.GetCharList`로 읽는다.

## TCP wire format

근거: `CoreLib/PacketTypes.h`, `CoreLib/PacketWriter.cpp`, `CoreLib/Config.h`, `ClientCore/PacketHelper/Packet.cs`, `PacketBuilder.cs`, `Config.cs`.

모든 구조체는 pack=1이며 현재 Windows 구현은 메모리 레이아웃을 그대로 직렬화한다. 정수는 little endian이다. 헤더는 8바이트: offset 0 `magic:u16=0xABCD`, offset 2 `length:u32`(헤더 포함 총 길이), offset 6 `opcode:u8`, offset 7 `flags:u8`. C01의 flags는 0이다. C++ enum OP 자체의 기반형과 헤더 opcode 폭을 혼동하지 않는다.

| 방향 / opcode | 총 길이 | 본문 필드와 절대 offset |
| --- | --- | --- |
| 인증 요청 / 1 | 53 | 8: userID u64, 16: sessionToken byte[37] |
| 인증 응답 / 2 | 9 | 8: resStatus u8 |
| 캐릭터 목록 요청 / 3 | 8 | 본문 없음 |
| 캐릭터 목록 응답 / 4 | 430 | 8: resStatus u8, 9: count u8, 10: CharacterInfo[10] |

CharacterInfo는 42바이트: 상대 offset 0 characterID u64, 8 name byte[32], 40 level u16. 이름은 UTF-8이며 첫 NUL 전까지 디코딩한다. 유효 항목은 count개이고 최대 10개다. 서버는 count와 무관하게 고정 430바이트 응답을 보낸다. 인증 토큰 UUID는 보통 36 UTF-8 바이트이며 37바이트 필드의 나머지를 0으로 채운다. 구조와 opcode 변경은 필요 없다.

TCP는 프레임 분할·결합이 가능하다. 현재 ClientCore 수신 버퍼는 프레임을 모으지만 magic/최소 길이/상한 검증이 없다. 특히 length=0이면 진전 없는 루프가 가능하다. C01 연결 경계에서 이를 검증하고 인증/목록의 고정 길이·count 범위도 확인해야 한다. 전투·프로필 등 다른 메시지 파서 개편은 이번 범위에 포함하지 않는다.

## 빈 목록과 실패 의미

`CacheLib/Handler.cpp:CharacterListRequest`는 `!res || !res->next()`에 **resStatus=0,count=0**을 반환한다. 따라서 DB 결과 0행과 조회 결과 없음/실패를 wire 응답만으로 구별할 수 없다. 1행 이상일 때 resStatus=1로 바꾸고 최대 10개를 채운다.

Coordinator 결정: 서버 의미를 바꾸지 않는다. C01 UI는 status/count와 응답 수신 여부를 구분하여 `캐릭터 목록 없음 또는 조회 실패`를 표시한다. status=0을 성공으로 바꾸거나 임의 캐릭터 생성·DB 삽입·시딩으로 통과시키지 않는다. 정상 로그인 → 인증 성공 → 서버 목록 응답과 일치하는 표시까지 실행 검증한다. 성공 응답이면 count개만 표시하고 0개 성공 응답이 실제로 오면 빈 상태로 표시할 수 있다.

## 최소 ClientCore 호환·수명 변경 후보

현재 API는 로그인 완료 Task가 TCP 인증·목록 도착까지 기다리지 않는다. UI는 목록 이벤트 또는 명시적 실패를 기다려야 한다. 기존 코드의 예외 삼킴·토큰 로그·명시 종료 부재는 Unity Play 종료/재시도와 비밀 없는 캡처를 막는다. 사용자 승인된 Unity 최소 호환 범위로 다음 파일만 구현자에게 배정할 수 있다.

| 후보 파일 | 필요한 동작 |
| --- | --- |
| `ClientCore/ViewModel.cs` | access/session token 로그 제거, 로그인·연결 실패 전달, 인증/목록 결과를 dispatcher를 통해 UI로 전달, 실패 시 이전 목록을 유지하지 않음 |
| `ClientCore/Interfaces.cs` | 위 실패/결과 전달에 필요한 최소 이벤트·명시 수명 경계만 추가 |
| `ClientCore/Initializer.cs` | 명시 Dispose에서 ClientTick.Stop·ReceiveTick.Stop 및 socket 종료 연결. Unity 종료 시 호출 가능하게 함 |
| `ClientCore/Network/TCPSocket.cs` | 명시 종료, 수신 루프별 socket/버퍼 소유, 닫힌 이전 연결이 새 연결에 영향 주지 않음, 기본 프레임 검증, 전체 바이트 송신 또는 실패 전달 |
| `ClientCore/Services/NetworkService.cs` | 연결·송신 오류를 정상 완료로 삼키지 않도록 전달 |
| `ClientCore/Services/AuthService.cs` | JSON null·필수 token·게임 주소/포트 검증. HTTP 계약은 유지 |
| `ClientCore/ViewData.cs` | 검증된 count개만 저장. 현재 foreach는 고정 배열 10개 모두를 저장하므로 유령 행이 생김. 실패 후 이전 목록 제거, UI 반환 값은 안전한 스냅샷 |

ClientTick/ReceiveTick에는 이미 Stop이 있다. 추가 변경은 구현 중 실제 필요가 확인될 때만 Coordinator가 범위를 정한다. 새 네트워크 계층·복제 ClientCore·일반 MMO 구조는 필요 없다. UI는 기존 ILogger/dispatcher/ViewModel/ViewData 경계를 연결하고, 버튼 중복 입력을 막으며 종료 시 이벤트를 해제한다. 일반 로그 문자열을 파싱하여 인증 상태를 추정하지 않는다.

## 필요한 검증과 복구

- 고정 변경 컴파일 후 실제 UI로 잘못된 입력 → 실패 표시 → 정상 입력 재시도, 인증 성공과 opcode 4 응답의 status/count 일치 확인.
- Play 종료·재시작 후 socket/tick/event가 남지 않고 새 로그인 1회당 결과 1회를 표시하는지 확인. 비밀번호·토큰 없는 PNG/GIF와 txt 실행 요약만 남긴다.
- 수신/송신 경계를 수정하면 분할/결합 프레임, 잘못된 magic·0/과대 길이·잘린 C01 본문·count>10 거부, 부분 송신의 처리처럼 실제 결함에 대응하는 좁은 검증을 추가한다.
- 서버·DB·프로토콜 변경 요청은 **없음**. 소스/런타임 차이가 실행에서 드러나면 원본 서버를 자동 수정/재빌드하지 않고 근거를 Coordinator에게 보고한다.
- 복구는 이 C01의 원본 ClientCore 변경과 Unity DLL/화면 변경만 대상으로 한다. 기존 사용자 변경을 되돌리지 않으며 서버·계정·DB 초기화는 필요 없다.
