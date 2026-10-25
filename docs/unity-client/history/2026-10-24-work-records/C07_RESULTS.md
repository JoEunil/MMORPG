# C07 인벤토리 조회 검증

구현: 가방 열기·조회·새로고침·닫기와 서버 슬롯/아이템 ID/수량 표시. 기존 opcode20/21의 81바이트 고정 계약을 유지한다. 성공 count만 반영하고, 실패·종료·시간 초과에서 과거 목록을 노출하지 않는다. 요청 중복과 시간 초과 뒤 같은 연결의 재요청을 막는다.

서버: 로비 진입 성공 시 이미 검증된 캐릭터 ID를 기존 세션에 기록했다. 실패 응답과 미사용 고정 배열을 초기화하고 count 초과를 안전 실패로 처리한다. DB/재화/아이템 규칙·새 프로토콜은 변경하지 않았다. 상세 독립 리뷰는 C07_REVIEW.md.

현재 실행 근거:

- Unity TestRunner PlayMode `C02.Tests.` 57/57 통과, 실패/skip0, 0.23초. 기존45와 신규12(파서·수명8, UI컴포넌트4)를 고정 버전에서 1회 실행했다. 실제 UI 입력 테스트와 구분한다. 원문 `Captures/c07-unity-regression.txt`.
- ClientCore DLL 빌드 오류0. NuGet 취약성 메타데이터 조회 NU1900 네트워크 경고가 있었다. Unity 최종 컴파일 오류0. 새 회귀 코드의 using/정적 도우미 호출 누락은 기능 테스트 전에 바로잡았다.
- Scene 저장 전후 기존 YAML 객체 ID 비교: 기존 객체 삭제0, 변경2(Canvas GameObject/RectTransform), 신규57. diff의 일부 지면 객체 이동 문맥은 직렬화 순서 변경이며 기존 객체 본문 변경은 아니었다.
- 서버 빌드: 초기 C1041·C1902는 격리 디버그 파일 설정 문제였다. PropertyGroup만 변경한 시도로는 ClCompile 설정이 바뀌지 않았다. 실제 ClCompile 메타데이터에 /Z7을 설정해 PDB 의존을 제거했다. 직접 프로젝트 빌드의 SolutionDir 누락도 수정했다. 최초 실패 로그는 보존했다. 최종 빌드·서버 회귀·실제 입력 결과는 아래 후속 확인으로 기록한다.

한계: READ ONLY 조회로 전용 캐릭터5011/5012 모두 characters_inventory 행이 없음을 확인했다. DB 생성/재시딩은 하지 않았다. 따라서 실서버의 성공 목록·빈 가방 성공은 미검증이다. 성공/빈/10행 표시는 fixture 근거이며 실제 계정의 성공으로 보고하지 않는다. 이번 실제 입력은 status0 응답·명시적 재조회·정상 종료를 검증한다.

최종 서버 빌드 및 회귀: 격리 UnitTests/MainServer 빌드 exit0. 처음 UnitTests 실행은 gtest.dll 누락(0xC0000135)으로 테스트가 시작되지 않았다. 기존 R01의 동일 런타임 DLL을 복사한 뒤 실제 NonZone12+Network12+Session3+PacketView4=31/31을 한 번 실행해 통과했다(10ms). 초기 실행 실패를 `c07-server-launch-missing-dll.txt`, 실제 결과를 `c07-server-regression.txt`로 구분해 보존했다. UnitTests SHA8BD491F9… / MainServer SHA9F5C4222….

실행 전환: R01 PID21008은 q helper exit0·native server exit0·포트 해제·빈WAL 확인 후 종료됐다. 새 오류 로그로 기존 전환 guard가 중단해 자동 후보/복구 기동을 하지 않았다. 독립 분석에서 새 로그는 종료 시각의 GQCS995 8개+AcceptEx10038 8개로, 기존 IOCP가 닫힌 listen에 accept를 재등록하는 경로와 일치했다. 이 NetLibrary 코드는 수정하지 않았다. cacheMQ3/DBworker4 종료, game.log 동일, 직접 캐릭터 READ ONLY 값 동일, WAL0을 재확인했다. 원문 로그를 보존하고 같은 안전 확인을 유지한 start-only C07 모드로 후보를 1회 기동했다. PID27540 / StartUtcTicks639279835005648240 / 9999준비 / DLL경로·hash 확인 / 시작error.log·stderr0. 중단 guard의 조건을 낮추거나 강제 종료하지 않았다. 상세 C07_REVIEW.md.

Git: 검증 전/최종 승인 전 원격 push나 PR 갱신은 하지 않는다. 기존 PR27의 C06 승인을 C07에 확대하지 않는다. 복구 기준은 C06 커밋7c83226 및 유지한 R01 실행 이미지다.

## 실제 입력 최종 결과

합성 마우스 A/B/D는 첫 계정 필드 선택, C는 계정 입력 뒤 암호 필드 선택에서 실패했다. 서버 로그인 전에 중단됐고 원문을 보존했다. 단순 입력 flush·Dynamic 단계 스케줄링 두 보완도 해결하지 못했다. 전담 진단은 실제 Mouse와 합성 Mouse가 UI 모듈에서 한 포인터로 합쳐지는 구조를 확인했으나 실패 원인으로 확정하지 않았다. 효과 없는 공용 C02 도우미 변경은 HEAD로 원복했다. 임시 입력 설정도 원복했고 제품/패키지/프로젝트 설정은 바꾸지 않았다.

다른 입력 수단인 Windows Computer Use 실제 마우스와 C07 수동 서버 관찰을 사용한 `c07-physical-e`를 완료했다. 실제 마우스로 계정/암호 필드 선택·로그인·서버 캐릭터5011 선택·ENTER·BAG·Refresh·Close·재열기·Disconnect를 수행했다. 자격 증명은 DPAPI에서 메모리로만 읽어 GameView 문자 이벤트로 입력했으며 두 문자 입력 assertion이 통과했다. 도구 인자/로그에 비밀 값을 남기지 않았고 account Clear를 호출했다.

실서버 inventory status0 응답을 정확히3개 관찰하고 각각 실패 안내/빈 rows/데이터 snapshot0/서버 위치 불변을 확인했다. 정상 UI 종료 뒤 Model=null·world hidden·observer 구독 해제·runner=false를 확인했다. PNG1280×720/446210bytes가 완성됐으며 root가 직접 열어 HUD·채팅·가방 비겹침, 읽을 수 있는 실패 안내와 실제 필드 상태를 확인했다. 원문 `Captures/c07-physical-c07-physical-e.txt` / PNG 동일 이름. 이 결과는 실서버 인벤토리 성공 목록 검증이 아니다.

정리: Play 종료/게임 연결0, 로컬 C07 서버27540·Unity32900·로그인12424·DB/Redis·수신기는 유지했다. 입력 라우팅 원래값 PointersAndKeyboardsRespectGameViewFocus, background=false. 정상 접속 종료에서 기존 CancelIoEx1168 WARN1건이 남았으며 별도 사실로 보존했다. 제품 변경이 없어 성공한 Unity57/서버31은 재실행하지 않았다.
