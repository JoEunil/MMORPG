# C02-AUTH-01/r1 독립 코드 리뷰

2026-10-18 KST. 판정: **코드상 차단 문제 없음**. 승인된 두 줄은 요청한 소유권·채널·삭제 상태 조건과 바인딩 순서를 구현한다. 전체 서버 빌드·실행 반영·실서버 소유권 검증은 완료되지 않았다. 리뷰어는 코드·DB·Editor·빌드·프로세스를 수정하거나 실행하지 않고 이 문서만 작성했다.

## 고정 기준과 승인 범위

| 파일 | SHA256 |
| --- | --- |
| CacheLib/Config.h | `E5AF1ABFEDC6219456940016485CC021A107B3ECF11A54186FB4B06956C0D51D` |
| CacheLib/Handler.cpp | `690CCE5F0A558F2709B567D4231D06AB270338B17A470994245982F2AB90A335` |
| UnityClient/Tools/Test-C02-EnterOwnership.py | `F2DB8DF08F338E19040F2C97DFC8DE784102F2CCF270E8F72FBAC83C21755B5F` |

SERVER_REQUESTS.md에는 지정 사람 사용자 U0C9QEUHPMK의 `!coord C02-AUTH-01 r1 승인`, ts `1792303113.782979` 원문이 기록돼 있다. Coordinator가 확인한 이 승인 범위는 두 소스 파일·로컬 빌드·비파괴 검증이며 현재 서버 바이너리 교체/재시작/배포를 포함하지 않는다. 현재 diff의 서버 제품 변경은 아래 두 줄뿐이다.

- Config.h QUERY_3: `c.char_id = ? AND c.user_id = ? AND c.channel_id = ? AND c.deleted_at IS NULL`.
- Handler.cpp CharacterStateRequest: `ExecuteSelect(3, body->characterID, body->userID, body->channelID)`.

DB 스키마·wire·동시 접속 정책·큐/락·기존 목록·다른 SQL을 확장하지 않는다. 테스트 도우미도 in-memory 합성 SQLite만 사용한다.

## 계약·수명 확인

- Enter wire에는 characterID만 있다. NonZoneHandler::GetCharacterState는 userID를 StateManager의 해당 sessionID에서 조회하고 channelID는 서버 CHANNEL_ID를 사용한다. 새 owner/channel 값을 클라이언트 요청에서 받지 않는다. 기존 NetPacketFilter의 MASK_AUTHENTICATED 확인은 미인증 세션의 Enter 전달을 거부한다.
- MsgCharacterStateReqBody의 characterID/userID는 uint64_t, channelID는 uint8_t다. DBConnectionGame의 ExecuteSelect는 statement3을 사용해 BindParams를 index1부터 호출하고 이후 인자를 순서대로 증가시킨다. BindOne의 uint64_t는 setUInt64, uint8_t는 setUInt로 전달하므로 세 조건 순서·형식이 SQL과 맞으며 이번 변경에 ID 축소 변환이 없다.
- QUERY_3의 SELECT 열과 profile JOIN은 유지돼 본인·해당 채널·active 캐릭터의 기존 char/profile/상태 매핑이 유지된다. 조건 미일치로 0행이면 기존 `!res || !res->next()` 분기에서 status=0 응답을 보낸다. MessageQueueHandler는 status!=0에만 로비 이민/상태 반영을 하고 기존 Enter 응답을 송신한다. 이번 변경은 새 성공 상태나 우회 실패 처리를 만들지 않는다.
- 기존 비동기 작업 안에서 SQL 파라미터를 먼저 바인딩/실행한 뒤 같은 메시지 버퍼를 응답으로 덮는 순서를 유지한다. 새 전달 계층·캐시·중복 상태는 없다. 보호 영역 승인 범위를 벗어난 추가 수정은 발견하지 못했다.

## 검증 근거와 한계

- Test-C02-EnterOwnership.py는 실제 Config.h의 QUERY_3 텍스트를 추출해 SQLite :memory:의 합성 행으로 own_active_correct_channel=1행, wrong_owner/wrong_channel/deleted/missing_character=0행을 검사한다. SELECT char/profile/version 매핑도 검사하며 소스 쿼리 대신 느슨한 대체 조건을 쓰지 않는다. 비밀·계정 파일을 읽지 않고 live MySQL 연결도 없다.
- `Captures/c02-auth-validation.txt`는 이 다섯 조건 PASS와 SQLite 합성 검증이라는 한계를 명시한다. SHA256은 `EE87D123FBFDDAFF1A08EFABB57C019B26C722CF9F39DA04CA2CD45E7AE8296D`로 인계와 일치한다. 리뷰어가 재실행하지 않았다. MySQL Connector 바인딩/실제 서버 통합 통과의 근거로 사용하면 안 된다.
- `Captures/c02-auth-build-retry.txt`에는 변경한 Handler.cpp 컴파일과 CacheLib.lib 생성이 기록돼 있다. 현재 lib SHA256 `413469EE38BE8606B92CD5341028DDCFEBB6FD6F07A4AA3D0640D9C421C9F911`도 인계와 일치한다.
- 같은 전체 MainServer 빌드는 ExternalLib.vcxproj:167의 기존 `packages/libevent2-vc142.2.1.8.1/build/native/libevent2-vc142.targets` 누락으로 실패했다. 이 실패를 전체 서버 빌드 통과로 보고하지 않는다. 기존 경고는 보존됐고 패키지 복구·프로젝트 설정 수정·런타임 교체는 리뷰 범위가 아니다.
- 현재 실행 서버에는 이 수정이 배포되지 않았다. 따라서 실제 C02 본인 캐릭터 진입을 기존 실행 서버에서 확인하더라도 새 소유권 차단 로직의 실서버 검증이 되지는 않는다. 전체 링크·실행 반영과 MySQL/본인 Enter 회귀는 별도 단계다. 임의/타 계정 ID 실접속 시험과 서버 재시작을 이 리뷰 판정으로 허용하지 않는다.

## Coordinator 추가 실행 근거 — 리뷰 이후
같은 두 소스 hash를 유지한 채 기존 정확 버전 libevent nupkg와 기존 external lib/DLL을 원본에서 읽기 복원했다. 프로젝트 설정/버전/원본 파일은 수정하지 않았다. 초기 package/importlib/postcopy 실패를 보존했고 최종 전체 MSBuild는 exit0(c02-auth-build-complete.txt), 작업 체크아웃 MainServer.exe SHA256 C0F689BD0E58D3A20282143D5DD5C0C15514F42F192A6FF70568C2DDF312E825이다. 이 문단은 Coordinator 실행 기록이며 위 리뷰어가 빌드를 재실행했다는 의미가 아니다.
별도 운영 C02-AUTH-DEPLOY-01/r1도 앱 요청에 대한 사람 승인(ts1792304457.908929)을 확인했고 정상 전환 도구를 검토 중이다. 아직 기존 실행 서버의 보안 조건 검증 완료로 표시하지 않는다.
