# C08 구역 이동 결과

기준 작업 커밋: `d7524822a8237cfec023f94cb18dc6abb8bb5a16` (`codex/unity-ai-client`). C08 구현·테스트 파일은 이 기준 위의 로컬 변경이며, 현재 결과는 해당 변경을 기준으로 한다. GitHub 반영은 최종 승인이 필요하며 실제 전달 상태는 GIT_DELIVERY_STATUS.md와 HANDOFF.md를 따른다.

## 구현

기존 Zone 1↔2 격자 경계 이동을 연결했다. 서버는 실제 이웃·경계 좌표를 확인하고 이동 중 세션 종료와 구역 변경을 보호하며, 새 방문 세대가 붙은 ACK와 시뮬레이션 패킷으로 이전 구역의 늦은 상태를 구분한다. 클라이언트는 확인 ACK 뒤 본인 캐릭터의 새 Full 상태를 받기 전까지 이동·스킬 입력을 잠근다. 좌표·HP/MP·쿨다운을 복사하고, 반대 구역으로 되돌아갈 때 스킬 슬롯이 중복 누적되지 않게 했다. 프로토콜은 ClientCore와 서버가 함께 바뀌어야 하며 구형 C07 서버와 혼용할 수 없다.

## 검증

- 서버 회귀 39/39 통과, 25ms: [c08-server-regression.txt](../../UnityClient/Captures/c08-server-regression.txt).
- Unity 첫 공통 회귀는 71개 중 69 통과·2 실패였다. 원인은 테스트 fixture가 초기 진입 응답을 전환 요청으로 세고, 인벤토리 UI 테스트가 실제 ViewModel 응답 경로를 건너뛴 점이었다. 기대 조건은 유지하고 fixture 경로만 수정·독립 리뷰한 뒤 영향받은 6개를 재검증해 6/6 통과했다. 첫 실패는 [원본](../../UnityClient/Captures/c08-unity-first.txt), 수정 후 결과는 [영향 회귀](../../UnityClient/Captures/c08-unity-affected.txt). 이는 서로 다른 실행의 결과이며 71개 전체를 수정 후 다시 돌렸다는 뜻은 아니다.
- TCP 경계 회귀 3/3 통과, 1.18초: [c08-tcp-regression.txt](../../UnityClient/Captures/c08-tcp-regression.txt). 첫 실제 서버 입력에서 구형 ACK 길이 검사가 수신을 막아 진입 실패했다. 길이 검사와 실제 분할·병합 TCP 회귀를 보완하고 독립 리뷰한 뒤 재검증했다.
- Unity TestRunner에서 최종 기준으로 성공한 서로 다른 fixture는 71+3=74개다. Unity 실행 기록의 총 실행 횟수는 71+6+3=80이다. 수정 전 2개 실패는 첫 결과에만 존재하며, 실패 수정 후 영향받은 6개를 통과했다. 수정 전후 실행을 합산해 모든 74개가 한 번에 통과했다고 표현하지 않는다.
- 실제 입력 배치 [첫 시도 기록](../../UnityClient/Captures/c08-physical-c08-physical-b.txt)에서는 물리 마우스로 Zone 1→2→1 이동은 됐지만 helper가 로그아웃 초기화 상태를 새 도착으로 오인해 정상 UI 종료 판정이 실패했다. observer를 고쳐도 실패를 성공으로 바꾸지 않게 보존했고, 수정 후 독립 리뷰했다.
- [최종 실입력 기록](../../UnityClient/Captures/c08-physical-c08-physical-c.txt): 재접속 후 이동 전에 확인한 Full 상태가 Zone 1, 좌표(101.084167, 10.7436523), HP 9975/MP 10000으로 저장 상태와 일치했다. 물리 입력으로 Zone 1→2 이동, Zone 2 이동, Zone 1 복귀를 수행했고, Zone 1 복귀 및 일반 UI 연결 종료까지 통과했다. [Zone 1 캡처](../../UnityClient/Captures/c08-physical-c08-physical-c-zone1.png), [Zone 2 캡처](../../UnityClient/Captures/c08-physical-c08-physical-c-zone2.png).

## 한계와 운영 상태

구현·리뷰·로컬 검증은 완료했으나 C08은 로컬 검증된 변경이며 GitHub 반영 승인은 아직 받지 않았다. 공개/배포되지 않았다. 정상 서버 전환 중 기존 네트워크 종료 경로의 AcceptEx 오류 경고가 재현되어 기존 중단 보호를 유지했다. 독립 원인 검토 후 후보 서버는 종료 작업 없이 start-only로 실행했다. 경고 원인 코드는 이번 C08에 포함하지 않았고 기존 종료 때 다시 나타날 수 있다. 전체 C++/Unity 컴파일 로그의 기존 변환 경고도 남아 있으나 이번 기능 회귀는 통과했다. DB 직접 변경은 없었다.


최종 정리: Play/changing/compile/dirty=false, 게임 established0. 두 PNG(1280×720) root 검수 완료; 버튼/채팅 패널 비겹침. 최종 READ ONLY 저장 행도 Zone1 (103.06175231933594,11.042296409606934), HP9975/MP10000으로 퇴장 상태와 일치했다. 재접속의 최초 raw Full 패킷 자체는 별도 계측하지 않았고 이동 전 확인한 Full 기준이다. Final server35672/9999와 로그인·DB·Redis·Editor는 유지한다. 정상 퇴장의 CancelIoEx1168 경고3건(초기실패·b·c)은 보존한다.

동결: DLL77B37C69725C748B94CF21E3C754801723A355DF2792D752B7FC2ECCACA48E8B, Scene1BBA91417CFE309C2EA71381CCB9B3254EAF7E80980648E2D53088EB75467E5E, observerF2982BDFE1656C5BF89FE3C9A7B353FB901605F19D46429D5C27356A8533551C. TMP fallback은 이동 버튼 화살표 U+2190/U+2192 두 glyph의 Unity 생성 atlas와 직렬화 갱신이며512×512/scale1/ClearDynamicDataOnBuild 유지, 새 폰트나 패키지 없음.
