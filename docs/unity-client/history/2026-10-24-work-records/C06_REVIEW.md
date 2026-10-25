# C06 독립 리뷰

2026-10-19. 결론: 코드상 차단 문제 없음. 최종 제품·helper 고정본의 정적 재검토를 완료했다. 이 리뷰는 정적 검토이며 기능 테스트 통과가 아니다. 제품 수정·Unity 조작·기능 실행은 하지 않았다.

## 지적과 해소

- P2 DynamicOS TMP_FontAsset 수명: C06ChatScreen이 생성한 자산에 OnDestroy 정리가 없었다. Coordinator가 Destroy(localKoreanFont)와 null 정리를 추가한 것을 재검토했다. 설치된 TMP 자산의 OnDestroy가 종속 atlas/material을 정리하므로 이중 파괴를 추가하지 않는다. 해소.
- P2 미응답 이름조회: pending 한 batch가 영구 점유할 수 있었다. 최종 ViewModel은 monotonic 5초 뒤 다음 dispatcher traffic에서 현재 pending 관찰 버전만 누락 억제하고 다음 요청을 진행한다. 실제 반환된 pending ID이면서 요청·최신 관찰 버전 이상인 항목만 완료한다. 늦은 빈 응답이 새 batch를 해제하지 않는다. 해소. traffic 자체가 없으면 정리는 다음 traffic까지 미뤄지지만 새 chat/full/delta도 같은 경계를 거친다.
- P2 초점 테스트 조기 통과: 키 입력 직후 MP/위치 동일만 검사했다. Coordinator가 자기 server action phase 관찰과 2초 후 위치/MP 불변·phase0 판정을 추가했다. 해소. 이 결과는 2초 서버 관찰이며 실제 모든 패킷의 미송신을 직접 계측한 결과라고 표현하지 않는다.
- P1 helper 컴파일: `C06LiveChatUiTests.cs`의 ActionResultField를 위해 `using ClientCore.PacketHelper;`가 필요하다. 동시 수정 중 이전 스냅샷을 읽은 것이었고, Coordinator의 갱신 후 최종 고정본 line10에 해당 using이 있는 것을 직접 확인했다. 최종본 해소.

## 확인한 경계

- 기존 Canvas 자식만 찾아 생성하는 설치 도구이며 같은 이름·부모의 자식을 재사용한다. 전체 씬 재빌드가 없다. Editor delegate onValidateInput은 런타임 직렬화되지 않지만 송신 시 UI/Core가 NUL/줄바꿈을 다시 거부하므로 송신 경계가 사라지지 않는다.
- UI는 실제 Global 수신 이벤트만 최근 12개 메시지에 보관하며 로컬 성공 에코를 만들지 않는다. 송신 예외 때 입력을 유지한다. chat log/input rich text가 비활성화된다. UI·Core의 공백/NUL/CR/LF/128 UTF8 byte와 엄격 surrogate 검증을 확인했다.
- 14/15/25의 헤더·배열 길이·offset/count·엄격 UTF8 검사와 24 가변 요청은 현재 서버 packed wire에 맞는다. malformed C06 프레임은 Handler에서 재throw해 TCPSocket의 기존 연결 해제 경계로 전달한다. 새 서버 제품 diff는 없다.
- profile 캐시·관찰 버전은 기존 dispatcher에서 갱신한다. 낮은 응답은 높은 관찰 버전/캐시를 덮지 않는다. disconnect/dispose 후 큐에 남은 chat/profile 콜백은 무시한다. 기존 fake의 Profiles noop 추가 외 C02/C04/C05 assertion 변경이 없다.
- 실제 peer는 전용 계정2/캐릭터5012만 선택하고 준비·대기 기한을 둔다. 본문·실제 server chatID와 echo 횟수를 확인하며 정상 dispose한다. DPAPI 자격증명은 메모리에서 처리하고 예외·stdout에 비밀을 기록하지 않는다.
- native helper는 기존 로그인/선택·GameView 입력 경로를 재사용하고 129B 입력 보존/자기·상대·한국어 echo/literal history/정상 UI disconnect를 관찰한다. 새 observer·테스트 장치·runInBackground는 finally에서 정리한다. 기존 실행 도우미의 실패 경계와 root의 Play stop/selector 정리가 배치 종료에 필요하다.

## 고정본 근거

아래는 최종 정적 검토 고정본 SHA256이다. helper using 추가도 이 해시에 포함됐다.

- ViewModel: `601473A888EE8A4C3410B8DB0CE838892581AAFDDCB623716325F55DFAFA6556`
- Packet: `B8791E67C278430B9567C19013AC2417CF474129C55A6181FBBFDA6AFB049E52`
- Handler: `3C8CC1C845B2B5C59B3F56E6F82C5F8E6513CB1513FFC47F11AD1F265C6DDFC6`
- TCPSocket: `044C3792DB24DFE8B99B0D4A5AD59F00DCC37920ABAD77509AFBCCFB3521DEE6`
- C06ChatScreen: `6552D7693ADB728A0224E671312A3E8F5E9CEE8C8000E66B7A23A07FC01B0675`
- C06ChatSetup: `0FEAD3911807AC946884D47E73867AFE540DFD887B5B0FE20E7D294B0A2BEE6F`
- C06ChatPacketTests: `74623EC214209DC7126E74DFBDD2A2A00231BE0BADDC237CE77A404429D1FEE6`
- C06Peer: `957CFF33097E5F56E13AFB81E067BB6EC660CEB1ADF78F47090B5074A615F178`
- C06LiveChatUiTests: `594FDE9E5D6EBCF6D77148FCA52DAFA753E24A0EA376AFF6E1E815B9D70307C9`

실행 미검증: Unity 컴파일, wire/profile fixture 실제 결과, 서버 echo·교차 수신, 한국어 글리프·패널 가독성, native 포커스, 정상 종료 정리. 기존 서버 Global key 미초기화 후보는 현재 단위에서 변경하지 않았으며 실제 전달 실패 시 별도 원인 분석한다. 수신 이름 표시와 프로필 누락 fallback이 실제로 관찰됐는지는 최종 배치 근거로 구분한다.


## 화면 보완·짧은 Slack 승인 수신 delta 재검토

2026-10-19 안전 경계 이후 기록. 결론: 검토한 delta의 코드상 차단 문제 없음. 제품 코드·서비스·Unity 입력·테스트를 리뷰어가 직접 실행하거나 수정하지 않았다.

- `C06ChatSetup`은 비활성 WorldHud를 현재 Canvas의 직접 자식으로 찾으므로 Play 이전 비활성 HUD에서도 설치 guard가 동작한다. history padding을 `(14,14,44,82)`로 바꿔 입력란·상태줄 위에 표시한다. 서버/ClientCore 변경은 없다.
- `StartReadability`는 기존 native 로그인·입력 경로로 실제 서버 메시지 3개를 보내고, 수신한 세 본문 모두 history.text에 존재하는지, TMP overflow가 없는지, history 하단이 input 상단보다 높은지 확인한다. 처음 제시한 helper에는 실제 UI 본문 assertion이 빠졌으나 최종 E2AA7287 버전에 추가됐다. 테스트 종료 시 observer·client·testMouse·background 정리를 유지한다.
- Slack receiver의 작은 호환 수정은 raw 단일 행 `승인 <짧은 대문자 ID>`만 추가 인식한다. 기존 !coord prefix 처리, 고정 workspace·사람·채널·등록 thread, bot/edit 제외, reservation 저장 후 ACK, 회신 ID 중복 억제, 실패 시 needs_review를 유지한다. 짧은 명령은 원문 그대로 전달하며 승인 유효 범위를 스스로 판정하거나 도구 권한을 늘리지 않는다. 따옴표·멀티라인·부가 명령·공백 변형은 거부한다.

최종 delta SHA256:

- Setup: `14FD4BDCD2AA06E58E73B4271F687F3E81DBB3D4F82B19E3C49EE2F6FBFF78F3`
- Native helper: `E2AA7287D6CAC3CDDADA28CA9C2030EF4471D30652E4387008DCF90D310F8087`
- Receiver: `DB8B39F9DCCF34625C2DECA2FC6987C1235956048BFBE0395E70FC8A57DA79AB`
- Receiver tests: `7FB8873D9DBEF587AF7BE4A0E895B08261E0086EFDBFC2FE9612270061D3F1EC`

실행 결과는 Coordinator의 관찰 인계로 기록한다. 최초 공통 배치 45/45 PASS와 실제 native 자기·상대·한국어 echo, 129B 거부/입력 보존, 입력 중 키1/2/3 관찰 PASS, peer PASS/disposed=1. 첫 PNG에서 history와 입력란의 겹침을 발견해 그 증거를 보존한 뒤 위 화면 delta만 수정했다. 영향 검증 `c06-20261019-layout`는 실제 세 본문 존재·overflow false·rect 비겹침 PASS이며 Coordinator가 새 PNG에서 세 줄의 완전한 표시를 직접 확인했다. 기존 wire/core/peer 테스트를 불필요하게 반복하지 않았다. 별도 수신기 회귀 12/12 PASS(85.6ms), 기존 receiver 정상 종료 후 동일 권한·상태로 재시작했다고 인계됐다. 최종 scene `B95D847D195F69131875B6A33D25718199D1263774649A523BEE724FE2592571`.

종료 상태도 Coordinator 인계 기준: Play/runner/background/model=false, selector=null. 위 PASS는 정해진 실제 3줄과 정상 입력 범위의 근거이며 최대 12개 메시지 모두의 화면 가독성·부하·모든 해상도·네트워크 미송신 직접 계측을 뜻하지 않는다. 중요한 ClientCore 후보의 push/PR/공개 경계는 여전히 Coordinator의 최종 사용자 승인 대상이다.
