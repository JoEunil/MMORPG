# C06 검증 결과

2026-10-19. 기준 첫 로컬 스냅샷 `a2296d9` 위 C06 변경. 서버 제품/실행 이미지01C626…는 변경하지 않았다.

- Unity TestRunner PlayMode 공통 배치 `C02.Tests.`: **45/45**, 실패/skip0, 0.24초, 1회. 신규 wire/profile10개와 기존 진입·Full·이동·관찰·전투35개. 길이/오프셋/잘린 프레임/UTF8/128바이트/버전/누락/늦은응답/수명을 확인했다.
- 실제 native 입력 `c06-20261019-a`: 계정1 UI 로그인·선택·진입,129바이트 전송 거부/입력 보존, 초점 상태 키1/2/3 뒤2초 동안 서버 위치·MP 동일/본인 phase0. 본인server chatID4 에코, peer 계정2 chatID3 응답, 한글 UTF8 에코와 richText=false 리터럴 표시, 정상 disconnect **PASS**. 직접 UI handler/패킷 주입/로컬 성공에코 없음.
- C06Peer: 상대1회수신·1회송신·자기echo1회·normal-dispose1 **PASS**. 비밀은 DPAPI에서 프로세스 메모리로만 읽었고 인자·출력·저장소에 쓰지 않았다.
- 최초 PNG에서 채팅 마지막 줄이 입력란에 일부 가려졌다. history padding만44/82로 보완하고, 영향 화면만 `c06-20261019-layout`으로 확인했다. 실제 서버 한글3본문이history에존재/overflow=false/rect비겹침/normaldisconnect **PASS**, root가 새 PNG3줄을 직접 검수했다. 통과한45개/peer교차송수신 전체는 반복하지 않았다.
- 짧은 Slack 승인 라우팅 단위: 기존7+신규5=**12/12**, 85.6ms, 1회. 정확한 한줄 `승인 C06`과 기존!coord를지원하며 workspace/채널/사람/등록thread/bot·edit/중복예약 경계 보존. 실제 짧은 사람 회신 왕복은 아직 미관찰이다. 기존 receiver는 stop marker→STOPPED 확인 후 같은 대상/기존state로 정상 재시작했다.

정리: runner/background/model=false, selector=null, Play/changing/compiling/scene dirty=false, 현재Unity console ground truth 오류0·경고0, 게임접속0. 과거buffer의4오류는B05초기compile,7경고는기존obsolete API 등이며 버퍼를지워숨기지않았다. 현재게임21008/login12424/Editor32900/DB5584/Redis4376 유지, receiver17604/node13856 LISTENING/stderr0.

증거는 로컬 `UnityClient/Captures/c06-regression.txt`, `c06-live-c06-20261019-a.txt`/png, `c06-peer-c06-20261019-a-result.txt`, `c06-live-c06-20261019-layout.txt`/png, `slack-short-approval-tests.txt`. 원본 미디어는 Git 제외이며 공개 HTML은 제작하지 않았다.

고정값: ClientCore DLL `B807463D36E9CA772A06020E5327382FB930BAB55F0746EC5861E0F7FCBE6DFB`, 최종Scene `B95D847D195F69131875B6A33D25718199D1263774649A523BEE724FE2592571`, helper `E2AA7287D6CAC3CDDADA28CA9C2030EF4471D30652E4387008DCF90D310F8087`. 독립리뷰 C06_REVIEW.md.

제한: Global만UI지원/귓속말UI없음. 이름 조회 전 첫 상대 줄은 서버chatID fallback이며 과거 줄을 나중 이름으로 재작성하지 않는다. 한국어는 현재 Windows의 Malgun Gothic DynamicOS 글리프 사용으로 타OS/없는폰트환경은미검증. history12개보관이지만 이번 실화면검수는3줄이며 긴history스크롤UI는없다. 기존서버 Global ChatEvent key미초기화와 CancelIoEx1168 경고는이번범위에서수정하지않았다.

복구: C06 소스·DLL·씬을 `a2296d9` 조합으로 되돌리고 빌드하면 C06 이전 검증 클라이언트를 복구할 수 있다. 서버이미지/DB 복구는 필요하지 않으며 DB 초기화/강제종료는 하지 않는다. 짧은 승인 라우팅은 별도2파일이므로 그두파일만되돌리고수신기를정상재시작할수있다.
