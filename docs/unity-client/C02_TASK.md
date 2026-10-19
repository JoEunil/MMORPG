# C02 — 캐릭터 선택과 첫 필드 진입

상태: C02 실제 사용자 흐름 수용(2026-10-18 KST). 클라이언트 구현·최종 독립 리뷰·C02 fixture10/10·C01 관련 회귀3/3, 실제 빈 목록 입력과 승인된 캐릭터 준비 후 실제 목록 선택·서버 진입·Play 재시작·PNG/정적6초GIF 통과. AUTH 두 줄은 별도 리뷰·로컬 전체 빌드 완료/실행 서버 미적용이며 운영 요청은 별도다. C01 Scene·기존 산출물은 보존한다.

## 목표와 소유권

기존 서버가 반환한 캐릭터 목록에서 선택한 ID로 월드 진입을 요청하고, 서버가 확정한 Zone·내 캐릭터 ID·좌표를 기본 도형과 고정 3D 쿼터뷰로 표시한다. 빈 목록·진입 실패·시간 초과는 정직하게 표시한다.

- 포함: C01 로그인/목록 화면 연결, 캐릭터 선택/진입 UI, 기존 Enter와 Zone ENTER 처리, 서버 좌표 `(x,y)`→Unity `(x,0,y)`, 열린 평면 필드와 내 캡슐 1개, 서버 확정 상태 표시·연결 수명 정리.
- 제외: 이동/전투/스킬/다른 개체 렌더링/채팅/인벤토리/거래소/아트 대량 제작/새 서버 계약. 서버가 자동 송신하는 메시지의 안전한 수신에 필수인 최소 기존 파싱 호환만 계약 조사 후 지정한다.
- 계약 분석 소유: c02_contract — C02_CONTRACT.md, SERVER_REQUESTS.md 및 승인용 후보만. 서버/DB/ClientCore는 읽기 전용이다.
- Editor 조작권은 Coordinator가 회수해 실제 CLI 실행을 맡는다. c02_implementation은 Play 중지 후 C02WorldScreen.cs의 발견된 수명 오류만 수정하며, c02_review는 고정 delta를 검토한다. 실행 중 제품/테스트 소스·에셋 쓰기를 멈춘다.
- 기본 구현 Luna/medium, 최초 계약·수명 등 복잡성이 확인된 부분은 PIPELINE_GUIDE의 Sol/high 예외만 적용한다. 전문 역할 동시 실행 최대 2개, 단일 제품 구현자·Editor 조작자를 유지한다.

## 승인 경계

### 계약 조사 후 확정 구현 범위

- 기준 계약: C02_CONTRACT.md. 서버 wire는 변경하지 않는다. C02-AUTH-01/C02-DB-01의 승인 대기는 독립 클라이언트 구현을 막지 않는다.
- 제품 구현자 단독 소유: ClientCore/PacketHelper/Packet.cs, Network/TCPSocket.cs, Network/ReceiveTick.cs, ViewModel.cs, Services/NetworkService.cs의 C02 필수 최소 호환 수정. 기존 Full 67바이트를 실제 43바이트로 정렬하고 해당 가변 길이·오류·종료 경계만 수정한다. Interfaces/Handler/ViewData는 실제 필요가 드러날 때 Coordinator에게 범위 확장을 보고한다.
- 필요 확인 후 추가 배정: Initializer.cs에서 ReceiveTick.OnFailed를 기존 ViewModel.Error에 연결·Dispose 시 해제한다. 기존 IHandlerTick에는 오류 전달이 없어 잘못된 패킷 재주입/억지 캐스팅을 피하기 위한 최소 수명 연결이다. 총 ClientCore 6파일이며 서버 보호 영역 수정이 아니다.
- 실행 중 발견한 Scene 파괴 순서 오류 수정: C02 OnDisable은 파괴된 UI를 갱신하지 않고 이벤트·연결만 정리한다. C01LoginScreen의 기존 UI 없는 ReleaseClient를 public으로 노출하는 1줄 접근 변경을 Coordinator가 허용했다. C02가 ConnectionFailed/SetBusy로 파괴된 C01 UI를 만지는 경로를 피하며 새 연결 종료 구조나 서버 변경을 추가하지 않는다.
- Full의 구형 charName 소비자인 Assets 밖 `UnityScripts/Game/PlayerManager.cs`·`PlayerState.cs`는 현재 UnityClient 빌드에 포함되지 않는다. 이번에는 수정하지 않고 후속 포팅 시 해당 컴파일 호환 조정 필요를 남긴다.
- Unity 제품 소유: Assets/C01/C01LoginScreen.cs 및 기존 SceneBuilder의 C02 연결에 필요한 작은 수정, 새 Assets/C02/ 런타임·Editor·Scene·관련 .meta, 기존 Build-ClientCore.ps1 실행으로 생성하는 단일 DLL. C01 Scene·테스트의 기존 동작은 보존한다. Scene·Prefab·Material은 지정 Editor API로만 생성/수정한다.
- 테스트 담당 단독 소유: Assets/Tests/C02/ 테스트·소형 Editor 입력 도우미와 .meta, Captures/c02-* 결과·미디어. C01의 Editor-update 실제 입력 방식과 비밀 없는 assertion 원칙을 재사용한다. 제품 구현자는 이 테스트 파일을 수정하지 않는다.
- 기존 이벤트를 재사용하고, Enter 성공만으로 필드를 표시하지 않는다. Zone 응답의 internalID와 동일한 Full을 모두 확인하고 ID·좌표를 표시한다. 자동 Zone ENTER를 UI에서 중복 송신하지 않으며 Full/Zone 도착 순서를 가정하지 않는다.
- 이번 구현은 Full wire 정렬·수신 스레드·취소 수명 경계 때문에 지침의 Sol/high 예외를 적용한다. Scene/에셋/Editor 조작권은 구현자에게 부여하고 테스트 실행 전 명시적으로 반납한다.

사용자는 승인된 기획 안의 작업을 계속하고, 별도 승인이 필요한 과정은 Slack에 남기도록 지시했다. 지정 스레드 `C0C9VFZMV43 / 1792249122.665229`, 지정 승인자 `U0C9QEUHPMK`를 사용한다. 사람의 요청 ID·개정에 대한 명시적 답변만 승인으로 판정하고 커넥터로 보낸 자체 메시지는 승인으로 보지 않는다. 수신기 대상 Coordinator는 변경하지 않는다.

기존 전용 계정 2개 로그인 권한은 유지한다. 캐릭터 생성/임의 DB 삽입/시딩 권한은 포함되지 않아 전용 계정별 1개 준비를 구체적인 별도 요청으로 만든다. 서버의 Enter 소유권 검사 누락 수정도 별도 보호 영역 요청이다. 승인 전 해당 서버/DB 변경은 하지 않고 독립 클라이언트 작업을 진행한다. ClientCore는 기존 wire 의미를 유지하는 필요한 Unity 최소 호환 수정만 허용한다. push·병합·공개는 여전히 별도 승인이다.

## 완료 조건과 검증 비용

1. 실제 UI로 서버 목록의 캐릭터를 선택해 진입하고, 성공 응답과 Zone 진입 결과가 모두 도착한 뒤에만 필드를 표시한다. 빈 목록에는 진입 불가 상태를 표시한다.
2. 내 ID/Zone/좌표가 실제 응답과 일치하고 캡슐을 해당 Unity 수평면 좌표에 표시한다. 클라이언트가 진입 성공이나 임의 좌표를 만들어내지 않는다.
3. 진입 실패/응답 지연/연결 종료에서 중복 진입·오래된 표시·이벤트가 남지 않는다. Play 종료/재시작의 직접 영향만 회귀 확인한다.
4. 같은 고정 변경을 독립 리뷰·필요한 계약 fixture·실제 Editor UI 입력 경로로 검증하고 txt·대표 PNG·짧은 상태/전환 캡처를 남긴다. C01에서 검증한 Editor-update 입력 방식을 재사용하며 NUnit 입력 제약 실패를 숨기지 않는다.

최소 검증은 관련 컴파일 1회, 변경한 Enter/Zone/자동 수신 프레임의 위험별 fixture, 실제 로그인→선택→진입 및 빈 목록/실패 상태, 관련 재시작 각 1회다. 테스트 소스/제품 소유권을 나누고 실행 중 프로젝트 쓰기를 멈춘다. 첫 실행/진단은 20분 상한, 필요한 조건이 통과하면 종료하며 새 변경/실패 때만 영향 검증을 반복한다. 같은 원인 2회 수정 실패 시 재현·로그를 Coordinator에 인계하고 작업 크기/모델을 재검토한다.

승인 대기로 실서버 진입을 못 하면 컴파일·fixture·독립 클라이언트 준비만 완료로 기록한다. 대체 계정·임의 캐릭터·가짜 월드로 실제 통과를 대신하지 않는다.

## 이번 검증 결과

최신 추가 결과: DB r1 사람 Slack 승인과 이 채팅 직접 커밋 승인 후 대상4행만 생성했다. 실제 UI run은 목록 status1/count1·본인5011·EnterCount1·독립 live Zone/Full observer의 Zone1/동일 internalID/좌표(10,10)·HUD/캡슐 일치를 확인했다. 같은 고정 제품과 helper로 Play 종료→재시작 후 재통과했다. 최초 행 클릭 실패는 보존하고 mouse 세 이벤트 사이 실제 Player frame 대기를 추가했으며 합격 조건을 완화하거나 handler를 호출하지 않았다. 최종 helper 독립 리뷰 차단 문제 없음.

실제 근거: Captures/c02-live-entry-first-pass.txt, c02-live-entry-restart-pass.txt, c02-live-character-list.png, c02-live-entry.png, c02-live-entry-state.gif. GIF는 실제 진입 후 정적 상태6초이며 전환/이동 영상이 아니다. 현재 실행 원본 서버에서 검증했고 새 AUTH 보안 패치의 실행 통과로 사용하지 않는다. AUTH local build exit0, 새 운영 승인 요청 C02-AUTH-DEPLOY-01/r1은 별도다.

아래는 캐릭터 준비 이전의 통과·미검증 이력이다.

- 최종 제품/도우미 컴파일 오류 0, 독립 리뷰 차단 문제 없음. 최종 소스 hash는 C02_REVIEW.md와 Assets/C02/Editor/BuildProvenance.txt에 기록했다.
- C02 PlayMode fixture 10/10 통과. 실제 CLI의 async 실행 후 test_status completed로 확인했다.
- 실제 입력 경로: 기본 마우스·GameView 문자 이벤트로 지정 계정 로그인, TCP 인증/목록 응답 1회, status=0/count=0, 10행 비활성·Enter 불가·월드 숨김·비밀번호 비움 통과. Editor-update 실행이며 NUnitRunner 결과가 아니다.
- 동일 제품의 별도 합성 확인: Zone/Full ID·좌표 표시, 컴포넌트 종료 후 월드/HUD 숨김·상태/모델 초기화 통과. PNG에는 서버 진입이 아닌 fixture라는 표시가 있다. 캡처 전 2프레임·최대 10초 셰이더 준비·1프레임 대기를 두었고 최종 평면·캡슐을 직접 확인했다.
- 근거: Captures/c02-test.txt, c02-empty-list-ui.txt, c02-component-disable.txt, c02-empty-list-ui.png, c02-fixture-preview.png. 서버 Enter 송신 0회. 실서버 non-empty 목록·선택·Enter/Zone/Full은 승인 후 다음 단계다.
