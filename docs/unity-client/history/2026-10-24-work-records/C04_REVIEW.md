# C04 독립 정적 리뷰

**최신 판정: 제품 및 초기 독립 Full 수신 대기 수정 helper의 정적 차단 문제 없음.** 초기 지적과 해시 이력은 보존하며 최신 기준과 실행 한계는 문서 끝의 재리뷰를 따른다. 실제 C03/C04 배치 통과를 의미하지 않는다.

2026-10-18 KST. **초기 판정: 차단 문제 있음 — P2 3건.** Coordinator 배정에 따라 제품·fixture·아트 파일을 읽었으며 이 문서만 작성했다. Unity/Editor/Play/테스트/프로세스/서버/DB 조작 및 제품 수정은 하지 않았다. 기존 C01~C03 미커밋 변경을 새 C04 변경으로 간주하지 않았다.

## 수정이 필요한 문제

1. **P2 — 파괴된 관찰 Visual 정리 중 예외.** `UnityClient/Assets/C02/C04ObservedWorld.cs:268–287`의 RemovePlayer/RemoveMonster/ClearObservations가 `entry.Visual.SetActive(false)`를 Unity-null 검사 없이 호출한다. Scene 종료 시 별도 C02World 루트의 자식 Visual이 먼저 파괴된 뒤 C02Client의 OnDisable 또는 WorldStateChanged 정리가 실행되면 MissingReferenceException으로 정리가 중단될 수 있다. 사전 파괴된 Visual을 포함한 disable/reset도 같은 경로다. 최소 수정은 Visual이 살아 있을 때만 비활성화·Destroy하고 dictionary 제거/초기화는 항상 완료하는 것이다. Visual 선파괴 후 정리 회귀를 추가한다.

2. **P2 — monster HP0가 같은 패킷의 ID 단위 유효성 검사를 우회.** `C04ObservedWorld.cs:217–221`에서 HP0를 발견하는 즉시 제거하므로 같은 ID의 dir4 또는 NaN 좌표와 HP0가 한 Delta에 있으면 잘못된 ID 후보를 무시한다는 계약과 달리 관찰이 삭제된다. 잘못된 필드가 앞에 있어도 후보를 지우고, 뒤에 있어도 이미 dictionary에서 제거돼 검사되지 않는다. 최소 수정은 ID별 후보에 사망 여부를 모으고 전체 필드 유효성 확인 뒤 제거하는 것이다. 유효한 HP0 다음 양수 HP가 같은 패킷에 있어도 Delta로 부활하지 않는 기존 기대는 유지한다. invalid dir/NaN+HP0의 양쪽 순서에서 해당 ID가 유지되고 다른 유효 ID는 적용되는 회귀가 필요하다.

3. **P2 — dispatcher 대기 패킷이 재구독된 새 세대로 전달됨.** `ClientCore/ViewModel.cs:223–252`의 player/monster Full/Delta 네 메서드는 Post할 때가 아니라 lambda 실행 시 현재 event delegate를 조회한다. 예를 들어 같은 VM에서 `MonsterFullReceived(old)`를 enqueue하고 C04 world close/open 또는 disable/re-enable로 generation을 바꾼 뒤 drain하면 새 closure가 과거 패킷을 받아 새 관찰을 생성한다. C04의 source/version 검사는 새 closure의 현재 값이므로 거부하지 못한다. 기존 fixture는 이전 delegate를 직접 저장·호출하는 경로만 검증해 이 경계를 빠뜨렸다. 최소안은 네 메서드에서 enqueue 시 delegate snapshot을 캡처하고, lambda의 lifetime guard를 유지하며 그 snapshot을 호출하는 것이다. 이전 C04 closure가 generation 검사로 거부하게 된다. 기존 monster 두 메서드에 더해 player 두 메서드의 좁은 수정이 필요하다. 실제 queued dispatcher로 enqueue→동일 VM 재구독→drain을 검증하고, 재구독 후 받은 새 패킷은 정상 적용되는지도 확인한다. 이는 wire에 세대가 없어 재구독 뒤 실제로 도착한 옛 Zone 패킷을 식별할 수 없는 기존 한계와 별개의 로컬 대기열 결함이다.

세 건 모두 Coordinator에게 보고했다. 수정은 구현 담당에게 재배정하며 이 리뷰에서 제품을 고치지 않았다.

## 확인한 범위

- player uint/monster ushort dictionary 분리, monster internal ID0 허용, own player 제외, type0/1/2 매핑과 `(x,0,y)`/기존 local-Z 방향 공유는 계약과 일치한다. 유효 Full만 생성·3초 표시기간 갱신, Full 부재로 즉시 삭제하지 않기, Delta의 미생성·미연장, player HP0 유지가 구현돼 있다. TTL은 서버 despawn 확정이 아니다.
- 최초 own Full에서 WorldStateChanged의 observedZone0 경로는 generation을 증가시키지 않아 이미 구독된 같은 player Full callback을 무효화하지 않는다. 실제 C02/C04 구독 순서와 최초 own+other 동시 패킷은 detached fixture만으로 검증되지 않았으며 공통 배치의 확인 대상이다.
- 모델 교체/월드 무효화/disable에서 네 delegate를 해제하고 source+generation closure를 다시 연결한다. Core monster 두 경로의 enqueue 전·실행 시 lifetime 검사는 존재한다. 위 3번 대기열 문제 외에 새 락/범용 계층은 추가하지 않았다.
- fixture 6개는 detached state/packet/lifetime 검사이며 실제 UI·서버 관찰 검증으로 주장하지 않는다. 기대를 낮추거나 비밀을 actual/expected/log로 출력하는 변경은 발견하지 않았다. 위 세 경계는 현재 fixture가 충분히 다루지 않는다.
- 최종 Scene YAML 내부 nonzero fileID 참조 누락 0개. 활성 C02Client의 enabled C04ObservedWorld 한 개가 login/world/displayRoot와 네 Prefab에 연결돼 있고 monster 배열은 type0/1/2 순서다. C02World/C04Observations는 identity transform의 빈 자식이다. 기존 선택 행 10개와 content topstretch `(0,1)–(1,1)`, sizeDeltaX0를 유지한다.
- 네 Prefab은 GameObject/Transform/MeshFilter/MeshRenderer만 포함한다. Collider/MonoBehaviour/UI component가 없다. 기존 기사 mesh/material을 재사용하고 신규 몬스터 재질·메시는 C04 경로에 작성하며 기존 재질을 바꾸는 경로는 발견하지 않았다. ApplyC04의 최소 binding과 전체 builder 재생성의 BindC04 호출이 일치한다. 가짜 게임 수치/이름 UI, AI, 충돌, 외부 리소스나 범용 EntityManager는 유입되지 않았다.

## 동결 기준 SHA256

| 파일 | SHA256 |
| --- | --- |
| Assets/C02/C04ObservedWorld.cs | `8C39A998867D5491F7B8543E31F47825315AAF4BB467CDFDDD2D7FFAFD3D51E9` |
| Assets/C02/C02WorldScreen.cs | `AA81E126DFB35FCC74F073DF532980B9ABA92C2E2719E44947B3AC7A6FED1F20` |
| ClientCore/ViewModel.cs | `705BEEE1AF188D952269A4FF3A40A18B1074E94B072C7E6420ED86BC1B1FD20C` |
| Assets/Plugins/ClientCore/ClientCore.dll | `394DFA80490B316CF68E41B15F4E7AD20DCEC9FD9A92940CFB7AD01B508329CA` |
| Assets/Tests/C02/C04ObservationTests.cs | `CD17CA6EF53CE306A8EA4B20787A88EC2372F651C890EF39C4AE73D5ABD8B213` |
| Assets/C02/Editor/C02SceneBuilder.cs | `D375A3E21FA8F1DBB6E8029154ADF4EC3C94FD0DDE518ADF1104D337BB42B674` |
| Assets/C02/C02.unity | `77F58BFD58360F4719CC9E86D55CB858C8D3F6014BABCCC7FB782D978CEEA23A` |
| Assets/C02/Art/C04/ObservedPlayer.prefab | `897F213CC8669C826FD8C06148F2E2D7C26E75203860677024BCDDC81EA1E453` |
| Assets/C02/Art/C04/MonsterType0.prefab | `07DEE3A12F8DF8CDE68923B3E7657143EA3BD073F36A7B259210590DC494297D` |
| Assets/C02/Art/C04/MonsterType1.prefab | `0D4E75B061C8FFE1A9AB43D1E87E7FDC3AACC2D1ADB640982F911F5AD2FA773F` |
| Assets/C02/Art/C04/MonsterType2.prefab | `9CC334A52C73FE2B6EDF9747656D5301C1B4EFBC531FB971DA45DBCD18967ECE` |

Assets 경로는 UnityClient 기준이다. Core 빌드 산출 DLL과 Unity 복사본 해시 일치를 확인했다. meta 및 세부 근거는 `UnityClient/Captures/c04-implementation.txt`, `c04-art-freeze.txt`를 함께 따른다.

## 초기 실행 근거와 한계

구현 담당의 원본 Core build exit0/errors0(기존 NU1900 경고2), 아트 담당의 자기 동결 범위 Unity compile completed/failedfalse/errors[]·Scene reload/dirtyfalse 기록을 읽었다. 이는 리뷰어 직접 실행이 아니며 이후 통합 helper 컴파일까지 통과했다는 뜻이 아니다. 아트의 쿼터뷰 PNG는 합성 Editor 미리보기이며 여기서 시각·실서버 통과로 판정하지 않았다. 현재 C04 fixture와 B01/B02/B03 실제 입력·이동·타인/몬스터 관찰은 미실행이다. 세 P2의 최소 수정·영향 fixture와 새 소스/DLL 해시를 받은 뒤 해당 delta를 재리뷰한다.

## P2 수정 delta 재리뷰 — 2026-10-18 KST

**코드상 차단 문제 없음.** 새 동결 소스와 추가 fixture를 파일로 읽어 기존 세 P2의 해소를 확인했다. 이 재리뷰도 문서 외 쓰기·빌드·Unity·테스트 실행 없이 수행했다.

- RemovePlayer/RemoveMonster/ClearObservations가 Unity-null 검사 후 Visual을 만지고 dictionary 제거/초기화는 항상 수행한다. 선파괴 Visual 개별 제거 및 displayRoot 선파괴 후 Clear/disable 양쪽 경로의 예외 없음·dictionary0 기대를 추가했다.
- MonsterDelta가 ID별 state/invalid/deathSeen을 수집한 뒤 최종 유효성 확인 후 사망 처리한다. dir/비유한 좌표 invalid는 같은 후보에서 유지돼 HP0의 위치와 관계없이 잘못된 후보를 무시한다. deathSeen은 유효한 HP0→양수 Delta의 부활을 막는다. dir4/NaN×HP0 앞뒤 네 경우의 원래 상태·활성 유지, 다른 ID의 정상 이동을 확인하는 fixture가 추가됐고 기존 유효 사망 기대는 유지했다.
- ViewModel player/monster Full/Delta 네 메서드는 enqueue 시 event delegate snapshot을 캡처한다. enqueue 전과 실행 시 lifetime guard를 유지한다. 실제 VM+QueuedDispatcher에서 이전 네 패킷 enqueue→world close/open→기존 ID 재생성→drain 후 옛 Full 생성/Delta 수정 거부를 검증하고 새 네 패킷의 생성/수정을 확인하는 fixture가 추가됐다. Dispose 회귀도 네 종류로 확장했다. 현재 구독자 캡처로 이전 C04 closure의 generation 거부가 작동한다.
- 새 API/wire/서버/인증 변경이나 새 범용 계층은 없다. Core의 player 두 메서드 추가 변경은 Coordinator가 필요한 최소 호환 범위로 배정했다. C02/Builder/Scene 해시는 이전 아트 동결과 동일하다.

직접 확인한 최신 SHA256:

| 파일 | SHA256 |
| --- | --- |
| Assets/C02/C04ObservedWorld.cs | `D7742F8CA56BFC819AFEB256FF3E3AC3001E97B730EFA0B0975B5211F7C5F5EC` |
| ClientCore/ViewModel.cs | `1D1808DBB79C206BB40E8586DEACD506CCEEBCD5D12A770E1C6246416BBF43F5` |
| Assets/Plugins/ClientCore/ClientCore.dll | `F5411C5EDA138CFCC151B0E6FF37BCA31F4BC3AFFA58EAA964CF1E46C11B72A8` |
| Assets/Tests/C02/C04ObservationTests.cs | `253D8EA82129BBB4AB544E9A497AD7D71DE4380881CA561B82F8AEEB602A836E` |

원본 Core 산출 DLL과 Unity DLL 해시가 일치한다. C02 `AA81E126…`, Builder `D375A3E2…`, Scene `77F58BFD…`도 직접 재확인했다. 구현 담당의 재빌드 exit0/errors0·NU1900 경고2는 `Captures/c04-implementation.txt` 근거다. 새 C04 fixture는 9개이며 아직 실행되지 않았다. 통합 Unity 컴파일·공통 B01/B02/B03 실제 입력·서버 관찰·최초 same-packet 구독 순서·시각 검증은 이 정적 판정에 포함하지 않는다. 새 UI helper/별도 peer도 이번 재리뷰 범위 밖이다.

## 실제 배치 helper/peer 추가 리뷰 — 초기 동결

파일 기반 검토이며 실행하지 않았다. 제품 동결은 위와 같다. 직접 확인한 추가 동결 SHA256:

| 파일 | SHA256 |
| --- | --- |
| Assets/Tests/C02/C04LiveBatchUiTests.cs | `E2F6EDAFC3963F4F208EA587EB70E55CE8A048A4B0C666A72E0985616622487F` |
| Assets/Tests/C02/C04LiveBatchUiTests.cs.meta | `0CEC0154AB134905519C4EFF610B6CF0FE026A933570815764E4F15808B2F8B5` |
| Tools/C04Peer/C04Peer.csproj | `D65B0C93DD9C440E688A0CA43F71C672513ADDF32DD6488782F86936D46F2EF3` |
| Tools/C04Peer/Program.cs | `0636E2A5874620DBF2513C5C2D390322F35956096E9608EFD53798FBD0A240AE` |

**실행 전 수정 필요(P2):**

1. `C04LiveBatchUiTests.cs:149`: 실제 `C02WorldScreen.worldHud` 필드 타입은 GameObject인데 RectTransform으로 직접 캐스팅한다. 항상 null이라 HUD 검사에서 실패한다. GameObject.transform을 통해 RectTransform을 가져오는 최소 수정이 필요하다.
2. `C04LiveBatchUiTests.cs:131–147`: 첫 목표 클릭 후 0.15초 대기만 하며 교체 직전 첫 목표가 아직 pending인지 확인하지 않는다. 이미 첫 목표가 완료된 뒤 별도 새 클릭을 해도 replacement PASS가 가능하다. 교체 직전에 moveTarget이 첫 목표이고 서버 위치가 아직 미도착임을 assertion으로 확인해야 한다.
3. `C04LiveBatchUiTests.cs:432–441`: 타 플레이어 확인은 저장 dir와 Visual.position만 검사하고 Visual.rotation은 검사하지 않는다. 방향 표시 오류가 통과할 수 있다. 이미 monster/own에서 사용하는 dir→Quaternion 회전 assertion을 player Visual에도 적용한다.

그 외 helper의 이동/선택/연결 종료는 MouseState neutral/down/up을 실제 Player frame에 나눠 전달하며 제품 handler/transform/이동 API를 직접 호출해 통과시키는 경로는 없다. reflection은 상태 조회와 기존 공유 runner 시작에 사용된다. 독립 Full/Delta 이벤트 관찰로 실제 entity 위치를 대조하고 본인 제외·disconnect 후 dictionary0·새 세션 Full 시각을 확인한다. HP0 실관찰 없음과 존재하지 않는 in-world text-focus 검증은 명시적으로 미관찰/미실행 처리한다. center type 미관찰 및 재진입 후 peer AOI 밖으로 TTL을 보지 못한 경우도 개별 NOT OBSERVED/NOT TESTED로 남기므로 최종 집계에서 그 항목을 통과로 바꾸면 안 된다. 이벤트/임시 Mouse/runInBackground/focus는 finally에서 복구하고 공유 runner는 120초 전체 제한 및 iterator cleanup을 제공한다.

peer는 별도 .NET9 프로세스이므로 Unity와 ClientSession static을 공유하지 않는다. DPAPI로 지정 계정2만 읽고 복호화 결과는 redirect pipe/프로세스 메모리에서만 소비한다. argv에는 복호화 스크립트와 경로만 있고 자격증명 값은 없다. QuietLogger 및 예외 상세 생략, readiness의 ID/Zone/좌표만 기록하는 경로에서 비밀 출력은 발견하지 않았다. 실제 목록의 단일 승인 ID5012를 확인한 뒤 그 반환 ID로 Enter하며 stop/EOF/5분 경로는 finally Dispose로 연결을 정리한다. 서버/DB/보호 설정 변경이나 강제 종료는 없다. peer는 UI 통과 근거가 아니다. compile0/helper loaded/Playfalse는 담당 전달 근거이며 아직 실제 로그인/입력/peer 실행 성공을 의미하지 않는다.

### helper 세 P2 수정 재리뷰

최종 helper SHA256 `643FEFAA174B90C0DEF33FC07503ECDDCF0614B2D20A91B64B71721A9F2628FD`를 직접 확인했다. meta `0CEC0154…`, peer project `D65B0C93…`, Program `0636E2A5…`도 초기 도구 동결과 동일하다. 해당 세 지점만 재리뷰한 결과 **실행 전 정적 차단 문제 없음**이다.

- HUD는 GameObject로 읽은 뒤 transform을 RectTransform으로 변환하므로 실제 제품 필드 타입과 맞는다.
- replacement 클릭 직전 pending target 존재·첫 목표와 거리0.001 미만·서버 잔여거리0.2 초과를 assertion으로 확인한다. 첫 목표 완료 후 새 클릭으로 replacement를 통과시키던 누락을 해소했다.
- player Visual.rotation을 실제 dir에 맞는 Quaternion과 각도0.01도 미만으로 비교한다. 저장 상태뿐 아니라 표시 방향을 확인한다.

기존 입력/서버좌표 기대를 낮추거나 제품 handler를 호출하는 변경은 없다. tester가 수정본 Unity compile completed/errors[]를 보고했으나 리뷰어가 실행한 검증은 아니다. 실제 Play/fixture/로그인/입력/peer 실행은 아직 미실행이며, 이 판정은 Coordinator가 동결 배치를 시작할 때의 정적 검토 근거다. 개별 NOT OBSERVED/NOT TESTED의 실제 한계는 그대로 유지한다.

### B01 첫 실제 실행 실패의 읽기 분석

이후 담당자가 실행한 B01은 기존 실UI helper `0BC34D8A7F0C9DC728A9CD7A710EE314BE5BB55C78A583B7213CE5E0EE0C3A13`에서 **실패**했다. `Captures/c04-b01-failure.txt`와 `c02-live-entry-ui.txt`에는 LoginIdInput 선택·ID 문자 입력 뒤 PasswordInput 클릭에서 selected=none/focused=false가 기록돼 있다. TCP 인증/목록/진입 성공이 아니며 C04 제품/새 helper/peer 통과 근거로 사용할 수 없다.

리뷰어는 기존 C01/C02 TypeText·Click·NextPlayerFrame, V02 builder와 저장된 `c04-b01-failure.png`만 읽었다. password 목표 좌표 `(1068,450.2)`는 1280×720 캡처의 실제 입력 필드 중앙과 일치한다. 입력 필드 Image의 raycastTarget=true, 장식=false 경로도 유지돼 정적 배치/레이캐스트 차단 결함은 발견하지 않았다. 실제 실패 시 RaycastAll 결과를 직접 실행해 확인한 것은 아니다. LoginIdInput이 이미 문자 입력까지 성공했으므로 전체 UI 입력 경로가 항상 끊겼다는 결론도 내리지 않는다.

GameView.Focus는 EditorWindow 포커스 요청이며 OS foreground 유지 여부를 직접 확인하지 않는다. 담당자는 Play 전에 editor_focus를 실행했지만 Play 후 helper 시작 직전 foreground/Application.isFocused는 기록하지 않았다. 따라서 이번 실패의 OS foreground 여부는 **미확정**이다. 설치된 Input System의 Editor/Player 별도 상태 버퍼와 focus별 이벤트 처리 때문에 Editor에서 읽은 mouseAtTarget=false 하나만으로 실제 Player 클릭 위치 오류나 특정 reset 원인을 확정할 수 없다.

현재 최소 권고는 제품·helper·assertion·입력장치·설정을 바꾸지 않고, 담당자가 Play 후 Editor OS foreground와 Game view focus를 확인한 뒤 동일 동결 helper로 **한 번만** 환경 재검증하는 것이다. 이는 성공을 보장하는 수정안이 아니라 원인 분리 재실행이다. 다시 실패하면 같은 원인 두 번째 실패의 로그·포커스·실제 UI hit 근거를 Coordinator에게 인계하고 반복 입력을 멈춘다. mock/handler 직접 호출·강제 TMP focus·입력 설정 완화는 제안하지 않았다. 리뷰어의 Editor/실행 조작은 없다.

### replacement 입력 타이밍 추가 한계

Coordinator의 추가 질문으로 `firstTarget=현재+3unit`/ClickWorldTarget/Wait0.15초의 실행 비용을 확인했다. ClientTick과 C03 요청은 50ms마다 최대1unit이고 서버도 50ms당 이동 예산1(cap3)을 준다. 공유 Editor runner가 MoveNext를 최소50ms씩 나누므로 neutral/down/up 및 중첩 iterator 처리 중 이미 이동하며 추가0.15초까지 지나면 첫 목표가 완료될 수 있다. 따라서 현재 pending assertion은 거짓 PASS를 막지만 그 전제가 안정적이지 않아 정상 제품에서도 실패할 수 있다. 이전 세 지점 재리뷰는 이 타이밍 안정성까지 확인하지 못했다.

현재 입력 배치 중 소스 수정은 하지 않는다. 추후 helper를 수정한다면 가시성 검사를 유지한 더 먼 첫 목표(예: 카메라 내6~8unit)를 택하고 불필요한 Wait0.15초를 제거하되 pending/첫 목표 일치/미도착 기대는 유지하는 최소안을 권고한다. 임의 위치 이동이나 handler 호출은 필요 없다. 이 전제 실패는 제품 회귀로 분류하지 않으며, 수정·재동결 시 해당 delta를 재리뷰한다. B01 기존 helper의 foreground 원인 분리와는 독립된 새 C04 helper의 검증 전제다.

### +8unit helper delta 및 실행 차단 기록

최종 전달 helper `F1EE7AED6E424AD33EA1B8E88061DCC488445765A5173D4B5FB2C655E1971D95`를 직접 확인했다. firstTarget +3→+8, Wait0.15초 제거 및 pending/첫 목표 일치/미도착 assertion 유지를 확인했다. ClickWorldTarget은 앞쪽 깊이와 화면 범위 검사를 유지한다. C02 camera offset(12,16,-12)/LookAt own+up/orthographicSize7.2에 따르면 1280×720에서 +8X는 대략 화면(923,135)로 화면 내·상단 HUD 밖이다. 이는 저장 소스의 투영 기하를 읽은 판단이며 실제 클릭/렌더 결과가 아니다.

그러나 공유 runner의 중첩 iterator 종료까지 세면 down 이후 NextPlayerFrame 실행·종료, up 실행 및 NextPlayerFrame 실행·종료, ClickAt 종료, ClickWorldTarget 종료, 호출자 pending 검사까지 최소 약8 Tick×50ms=0.4초가 걸릴 수 있다. 서버 Delta도50ms이며 8unit 최단 이동시간0.4초와 비슷해 **+8은 개선이지만 현재 목표가 여전히 pending이라는 전제를 안정적으로 보장하지 못한다.** 앞선6~8unit 제안에서 이 중첩 종료 비용을 충분히 세지 못했다. 전제 assertion 자체는 계속 거짓 PASS를 막으며 실패 시 제품 결함으로 분류하면 안 된다. 더 먼 화면 내 목표(+12X는 같은 기하에서 대략1064,41)로 여유를 확보하거나, 첫 down 처리 직후 전제를 확인하도록 한정 입력 구간을 분리하는 후보를 Coordinator에게 보고했다. 소스 수정은 하지 않았으며 변경을 배정할지와 실행 여부는 Coordinator가 결정한다.

별도로 Coordinator/담당 기록에 따르면 B01 환경 재검증은 자동 승인 검토가 직전 combined call 중복이라는 이유로 helper 시작 전에 거절했다. 관련 combined call 자체도 평가가 거절돼 수락된 helper 호출은 없고, 기존09:36 실패 파일 시각 유지·Running=false를 확인한 뒤 Play를 정상 중지했다. 이는 재실행 실패나 통과가 아닌 **실행 차단**이다. 다른 handler/도구 경로로 우회하지 않았고 재시도 근거 제시 여부를 Coordinator가 판단 중이다. 담당자는 helper 수정본 compile completed/errors[] 및 기존 fixture22/22 결과를 보고했으며, 제품/component/ViewModel/DLL/C04 fixture 해시는 이 리뷰에서도 기존 동결과 같았다. 리뷰어는 해당 실행을 직접 수행하지 않았다.

### 최종 +12unit 한 줄 delta 재리뷰

실제 파일 SHA256 `4C60550EB1785CBC905780B7FBA7B18D3A63CF5B668CF9EB4D5CB705627D154A`를 직접 확인했다. 전달 문자열의 `…B7BFA…`와 달리 실제 해시는 `…B7FBA…`이므로 이 파일 해시를 기준으로 기록했다. firstTarget +8→+12 한 줄과 pending/목표 일치/미도착/화면 가시성 assertion 유지를 확인했다. **해당 delta 기준 정적 차단 문제 없음.** 앞선 카메라 기하의 화면 내 목표에서 입력 종료와 도착 사이 여유를 늘리며, 느린 프레임 등으로 실제 전제가 성립하지 않을 때는 계속 assertion으로 실패하게 한다. 그 실패는 test 전제 실패로 기록하며 제품 회귀와 구분한다. 무한 안정성·영구 완료를 보장하는 판정은 아니다.

담당자는 Unity compile completed/errors[]·Playfalse·fixture22/22 유지·제품/DLL/Scene 불변을 보고했다. 보존된 `Captures/c04-b01-pass.txt`에서 실제 PasswordInput 선택 및 로그인/CharacterChoice0/Enter 클릭 뒤 서버 Zone/own Full 진입 assertion PASS(UTC09:42:08)를 읽었다. 이는 담당 실행의 B01 통과 근거이며 리뷰어 직접 실행이 아니다. 이전 실패와 시작 전 자동검토 거절 기록도 보존한다. 최종 +12 helper의 실제 C03/C04 이동·관찰 배치는 아직 통과로 기록하지 않는다.

### 첫 실제 phase1 실패 — 이동 중 화면 투영 전제

`Captures/c04-live-batch.txt`(UTC09:46:14 종료)를 읽었다. 실제 C04 peer ID3/Zone1/타인1·몬스터40의 서버 상태 대조, C03 near `(10,10)→(10.8,10.6)`와 far diagonal `→(13.3,12.1)`은 부분 PASS다. 이후 WaitFor assertion이 실패했으므로 phase1 전체·replacement·HUD/재진입은 통과가 아니다. 현재 기록은 WaitFor의 구체적 step 문자열을 저장하지 않아 두 replacement 대기 중 어느 것인지는 이 로그만으로 확정하지 못한다. helper 해시 `4C60550E…`는 유지됐다.

코드상 실제 테스트 oracle 문제가 가능하다. ClickWorldTarget은 월드 목표를 화면 좌표로 먼저 계산하고, 중첩 ClickAt의 neutral→Player frame→down을 기다린다. 그동안 C02 ShowPosition이 own 서버 이동과 같은 거리로 카메라를 평행이동하며 상대 시선은 유지한다. 그러므로 화면점이 고정돼 있으면 실제 down의 ground hit는 최초 목표에 그동안의 카메라 이동 Δ를 더한 위치가 된다. replacement 목표가 start−1.2인 현재 코드에서 전진 ΔX가1.2를 넘으면 실제 클릭 지점은 start 뒤가 아니게 되고, helper는 여전히 start보다 뒤로 이동하기를 기다린다. 이는 제품의 목표 교체 실패를 입증하지 않는다. 최초 목표가 pending이라는 assertion을 만족해도 이 별도 투영 전제는 보장되지 않는다.

최소 후보는 reverse 목표를 화면 내−6unit으로 넓히고 기존 가시성·서버가 실제로 뒤로 갔다는 기대를 유지하는 것이다. 다른 후보는 월드 클릭에서 neutral/down 직전에 목표를 동적으로 투영하는 한정 경로지만, queue 이후 실제 Player 처리까지 한 프레임의 카메라 이동은 남아 짧은−1.2에 대한 완전한 고정 hit 보장은 아니다. 요청 목표/실제 클릭 후 moveTarget/클릭 전후 서버 위치의 안전한 좌표 진단을 남기면 전제 실패와 제품 실패를 분리할 수 있다. 현재는 실제 hit 수집 전이므로 원인 가설이며 제품 수정 근거는 없다. 이 읽기 분석에서 source·Editor·입력·실행은 변경하지 않았다.

### reverse −6 및 좌표 진단 delta 재리뷰

최종 helper SHA256 `2F64FD3EC1AF7EBCF94EE0163672A60F2A1C2B4980137895CC8AAA22BB2BA21C`를 직접 확인했다.**해당 한정 delta의 정적 차단 문제 없음.** replacement 목표만−1.2→−6으로 넓혔으며 첫 목표 pending/일치/미도착, 클릭 화면 가시성, 서버 x가 replacementStart보다0.2 이상 뒤로 갔다는 WaitFor, 목표 종료 기대를 유지한다. 현재 카메라 기하에서−6X 화면점은 대략(428,463)으로 화면 내·HUD 밖이다. 기존 기대를 낮추거나 제품 handler/Transform을 조작하는 변경은 없다. start/requested 목표와 클릭 후 moveTarget/current server 로그는 좌표·null 여부만 출력하며 자격증명/패킷/세션 정보는 포함하지 않는다. 카메라 이동 여유를 늘리는 수정이며 임의 프레임 지연에서도 고정된 world hit를 보장한다는 뜻은 아니다.

첫 실패는 `c04-live-batch-first-failure.txt`와 `c04-phase1-failure.png`에 보존됐으며 부분 PASS와 WaitFor 실패를 유지한다. 담당자의09:47:12 읽기 관찰은 app focused/Play true/world visible/server(18.1,12.1)/moveTarget null/players1/monsters60이었다. 이는 실패 뒤 상태이며 실제 클릭 당시 hit나 어느 Wait가 실패했는지를 확정하는 근거는 아니다. 당시 peer는 나중에 PID 없음이 확인됐지만 exit code·정상 stdin 종료 여부는 미확인이므로 정상 종료 통과로 기록하지 않는다.

담당자는 Play 중지 후 helper 수정·Unity compile completed/errors[]·관련 fixture22/22 및 제품 해시 보존을 보고했다. 수정본의 실제 배치는 아직 미실행이다. 같은 카메라 원인으로 다음 실행도 실패하면 반복 수정/재시도하지 않고 Coordinator의 축소 진단으로 인계한다. 리뷰어는 문서 외 수정·Editor·테스트 실행을 하지 않았다.

### 두 번째 phase1 초기 실패 — 독립 Full 구독 초기화 race

이후 같은 `2F64FD3E…` 실행은 UTC09:52:28에 `ObservePeerAndWorld` 내부 assertion으로 실패했다. 최신 `c04-live-batch.txt`에는 관찰/이동 PASS가 없으며 이번 실행이 near/far나 replacement까지 도달했다고 기록하지 않는다. 앞선 부분 PASS/WaitFor 실패와 다른 단계다. 현재 로그는 assertion 위치·수신 개수를 생략하므로 실패 줄을 이 로그만으로 확정할 수 없다.

읽기 검토에서 helper의 실제 초기화 race를 확인했다(P2, 테스트 도구). lines69–72에서 독립 이벤트 수집기를 새로 구독하지만 line74 WaitFor는 기존 C04 player dictionary에 peer가 있는지만 본다. 제품이 구독 전 Full로 이미 peer를 표시하면 대기가 즉시 끝나며, 다음 Full 전 line78의 독립 players.TryGetValue가 실패할 수 있다. Core는 enqueue 때 delegate snapshot을 캡처하므로 구독 전 queued Full도 새 수집기를 채워주지 않는다. 이것은 제품 관찰 결함의 증거가 아니다.

최소 수정 후보는 같은30초 WaitFor 조건에 `players.ContainsKey((uint)expectedPeerId)` 또는 TryGetValue를 함께 요구하는 한 줄이다. 같은 peer의 실제 Full을 받기까지 기다리는 것으로, 뒤의 독립 상태/Visual 일치 assertion을 낮추거나 직접 값을 채우는 수정이 아니다. monster 대기는 이미 제품 dictionary와 독립 collector 양쪽 count를 요구해 이 빈 collector race와 다르다. 제품/peer/서버/프로토콜 수정은 필요 없다. 리뷰어는 수정·실행하지 않고 Coordinator에게 후보만 보고했다.

### 독립 Full 수신 대기 한 줄 재리뷰

helper SHA256 `DDB12233ADE07B2FED0F54E540EF99E9C78DB996A083267A4E497FFCC721496D`를 직접 확인했다. WaitFor가 C04 dictionary 존재와 `players.ContainsKey((uint)expectedPeerId)`를 함께 기다린다. 기존30초 제한·본인 제외·동일 peer의 독립 수신/저장 상태/Visual 일치 기대를 유지한다. **해당 초기 구독 race 수정 delta의 정적 차단 문제 없음.** 제품이나 수신 값을 직접 조작하는 변경은 없다.

Coordinator는 수정본 Unity compile completed/errors[]·Play 중지 및 이전 peer의 stdin 정상 종료/EXIT0 도구 확인·실패 캡처 보존을 보고했다. 이는 담당 확인 근거이며 리뷰어 직접 실행은 아니다. 위 peer 종료 미확인 표기는 이 후속 확인으로 갱신한다. 새 helper 실제 배치 결과는 아직 미실행으로 유지한다.

## 지정 orphan peer 정상 종료 도구의 실행 전 정적 리뷰

대상 `UnityClient/Tools/Stop-C04Peer.ps1` 최종 확인 SHA256 `FE9597FA84D3A2E36EAB9A3A665C37E9F6B20B781CCF4611CA20EF444F9E6A4B`. 읽기 중 초기본60ADA…→receipt 추가7104…→최종FE959…로 저장됐으며 최종 파일의 identity/Win32/receipt 경로를 다시 읽었다. **이 최종 파일 기준 정적 차단 문제 없음.** 스크립트 실행·native 호출·콘솔 연결·프로세스 조회/조작은 하지 않았다. 배정 범위는 첫 stdin 핸들을 잃은 테스트 peer 하나의 정상 종료이며 서버 종료 도구가 아니다.

- PID30528, UTC start ticks639279134932903697, worktree의 `C04Peer/bin/Debug/net9.0/C04Peer.exe` 절대 경로, SHA256 `64369E2C5468A00085EADA7E56B91EEA1B9F335A9B72DB8B7FC4987AFF805BA1`를 검사한다. 해당 파일 해시는 리뷰어도 파일 읽기로 확인했다. parent9652의 CIM 관계 및 `C:\Program Files\dotnet\dotnet.exe` 경로도 요구한다. 실제 살아 있는 프로세스의 일치는 Coordinator가 확인한 근거를 사용하며 리뷰어가 실행 확인한 것은 아니다.
- 대상 Process.Handle을 핀한 뒤 identity를 다시 검사한다. AttachConsole 후 target와 helper가 반드시 존재하고 추가 client는 지정 parent만 허용한다. Write 직전 이 콘솔 목록을 다시 검사한다. 다른 console client/과다 client/조회 실패는 입력 전 중단한다.
- KEY_EVENT_RECORD는 int BOOL·ushort 반복/키/scan·Unicode char·uint control로16바이트, INPUT_RECORD는 type offset0/union offset4/size20이다. Enter down/up 두 레코드만 CONIN$에 쓰고 기록 수2를 요구한다. 서버 종료 문자나 신호·강제 종료 코드는 없다.
- `-Send` 없이는 입력을 쓰지 않는다. 단 dry-run도 자기 콘솔 해제/대상 콘솔 연결·해제 및 receipt 기록은 수행하므로 무작용 읽기 전용이라고 표현하지 않는다. 실패하거나 정상 반환할 때 대상 콘솔을 finally에서 분리한다.
- Send 후 대상의10초 종료 대기와 핀한 handle의 GetExitCodeProcess를 사용하고 code0만 성공으로 끝낸다. timeout/조회 실패/nonzero는 실패이며 force kill은 없다. receipt의 시각/단계/종료 코드/도구 오류에는 비밀 값이 포함되지 않는다.

기존 peer의 Console.In.ReadLineAsync 뒤 Enter/EOF→EXIT0/finally Dispose가 의도한 정상 경로다. 콘솔 입력이 실제 stdin reader에 연결돼 있는지와 Enter 수신·실제 EXIT0는 정적 리뷰로 증명하지 않는다. Send의 성공 문자열만으로 정상 종료를 보고하지 말고 최종 대기/코드0 근거를 사용해야 한다. 도구가 실패하면 다른 대상/강제 종료 경로로 확대하지 않는다. Coordinator가 이후 실행을 담당하며 플랫폼 자동 검토는 별도 적용한다.

이 배정 시점에는 Coordinator가 DDB12233… 실제 phase1 및 Recorder 완료를 보고했고 보존 실행 로그의 phase1 완료 문구도 읽었다. 이는 종료 도구의 실행 검증을 대신하지 않으며 리뷰어 직접 Unity/peer 테스트는 없다. 초기30528 peer의 생존/정리 문제와 별도 peer의 이전 EXIT0 확인은 구별한다.

### 종료 도구의 검증된 부모 콘솔 계보 delta

최종 동결 `Stop-C04Peer.ps1` SHA256 `D2752CA557773190BFF1B6EF21EABFF88AB476CDFFDC1791E9EED0374D05EC20`을 직접 확인했다. **이 한정 delta의 정적 차단 문제 없음.** target30528→parent9652의 dotnet 경로 검증에 더해, parent9652의 CIM ParentProcessId320 및320의 정확한 bundled `codex-primary-runtime/dependencies/native/powershell/pwsh.exe` 경로 검증을 추가했다. 콘솔 허용 목록만 target/self/9652/320으로 늘리고 최대4명으로 제한한다. target/self 필수·그 외 PID 거부·Write 직전 재검사 및 기존 Enter 두 레코드/10초 대기/핀 handle exit code0 조건은 유지한다. 검증하지 않은 일반 shell이나 서버를 허용하는 변경은 아니다.

`Captures/c04-orphan-peer-stop.txt`를 읽었다. 기존 dry-run은4명이라 입력 전에 차단했고, 계보 확인 후 최종 dry-run(UTC09:59:50)은 `send=False`·identity 확인·exclusive clients4·no input sent였다. 이 결과는 Coordinator 실행 근거이며 리뷰어의 native/console 실행은 없다. 실제 Send/peer EXIT0 정상 종료는 아직 이 리뷰 범위에서 확인하지 않았고 root 담당 작업으로 남는다. Coordinator가 보고한 실제 phase1 전체, phase2 TTL/type0·1·2/최종 disconnect 및6초 Recorder 완료와 이 잔여 orphan 정리를 분리한다.

### 정상 종료 실행 확인 및 사후 peer 한 줄 수정

Coordinator 실행 후 보존된 `c04-orphan-peer-stop.txt`를 다시 읽었다. 최종 D2752CA5… 도구의 UTC10:01:00 `send=True`, exact identity·exclusive clients4·Enter down/up, 최종 `exit code=0`가 기록돼 위 정상 종료 미확인 상태를 갱신한다. Coordinator는 이후 elevated 조회에서 C04Peer0/9999 established0·기존 서비스 PID 유지도 확인했다고 보고했다. 리뷰어가 도구/native/process 조회를 실행한 것은 아니다.

테스트 완료·Play 중지 뒤 `Tools/C04Peer/Program.cs`는 stdin 대기만 `Console.In.ReadLineAsync()`에서 `Task.Run(() => Console.In.ReadLine())`로 바뀌었다. 직접 확인한 새 SHA256은 `0440BD76F0058989610FDA6E59D6FCD9E767CC4CBB15648E215012A10207BF1E`다. **이 한 줄 delta의 정적 차단 문제 없음.** 동기 Console reader가 호출 자체에서 대기하더라도 별도 ThreadPool 작업 안에 한정돼 Main의 dispatcher/5분 deadline 루프 진입을 막지 않는다. 기존 stop/EOF의 완료 판정, timeout 실패, EXIT 및 finally Dispose 경로는 유지된다. 새 네트워크/세션/비밀 출력이나 Unity 제품 변경은 없다. Coordinator가 dotnet build 오류0/경고0을 보고했으며 새 peer의 live 실행은 하지 않았다.

완료된 실제 배치는 **수정 전 peer Program `0636E2A5…`와 helper `DDB12233…`** 기준이다. 저장 실행 로그에서 phase1의 실제 서버 타인/몬스터·near/far/diagonal/replacement·HUD·disconnect/실UI 재진입 완료, phase2의 peer TTL/type0·1·2/최종 disconnect 완료를 읽었다. fixture22/22 및6초 Recorder 완료는 담당 실행 근거를 따른다. HP0 실서버 사망은 미관찰, 존재하지 않는 in-world text-focus 검사는 미실행이라는 기존 한계는 유지한다. 제품/DLL/Scene 및 helper 동결과 그 배치의 통과 적용 범위는 이 사후 도구 수정으로 바뀌지 않는다. **새 peer0440…는 빌드·정적 리뷰만 완료이며 live 재검증 통과로 표시하지 않는다.**
