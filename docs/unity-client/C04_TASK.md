# C04 — 타 플레이어·기존 몬스터 관찰 표시 구현

현재 지시: 사용자가 C04 구현과 Unity 입력 테스트 재개를 명시했다. 이전 입력 보류는 해제됐다. 저작·제품·테스트가 최종 동결/독립 리뷰된 뒤 B01+B02+B03을 한 번의 공통 실제 로그인/진입 배치로 검증한다. 저작 중에는 입력/Play를 실행하지 않는다.

계약: C04_CONTRACT.md 실제 playerFull43/Delta10·monsterFull25/Delta8을 재사용한다. monsterID0 유효, 본인제외, 종류별 별도dict. 유효Full만 생성/lastFullSeen 갱신,3초표시유효기간; Delta는알려진대상만갱신/TTL연장없음. Full 부재즉시삭제금지, monsterHP0숨김·Delta부활금지·살아있는Full재등장, playerHP0는삭제아님. 유한성/dir/HP·type검증과세션/Zone/disable정리·늦은callback거부. TTL은서버despawn통보가아니며 정확즉시삭제/생성순서는현wire로보장하지않는다.

범위: 타플레이어는기존V02성인기사visual재사용, monster실제0/1/2대표외형각1개와서버위치/방향표시. 이름조회·새HP/스킬/전투입력·몬스터AI·피해연출·UI시스템/새wire/서버/DB/운영·풀링/범용EntityManager 제외. HP라벨은최소범위로생략하며실제HP는내부상태검증/HP0정리에만사용한다.

소유권: c03_implementation은 C04ObservedWorld.cs/meta, C02WorldScreen.cs의월드가용성이벤트/공유방향함수최소연결, ClientCore/ViewModel.cs MonsterFull/Delta lifetimeguard, 원본빌드DLL, C04최소fixture/tests/meta와 c04-implementation.txt만. v01_artist는 C02/Art/C04 prefabs/materials/mesh/meta, C02SceneBuilder.cs의최소binding/재생성유지, C02.unity와제작README만. 서로다른파일병렬,타인변경보존. Editor는아트담당이Editmode저작/컴파일/preview만독점,구현자는소스작업만. 저작종료후테스트담당에게조작권인계하며배치중Assets/Packages/ProjectSettings쓰기전면중지.

직렬화계약: C04ObservedWorld를활성C02Client에추가. loginScreen(C01LoginScreen),worldScreen(C02WorldScreen),displayRoot(Transform:C02World/C04Observations),playerPrefab(GameObject),monsterPrefabs(GameObject[3] type순). visualroot는groundy0/localforward-Z/noCollider/noUIraycast. 기존C02본인Transform단독소유유지. C02 WorldStateChanged 이벤트는실제월드가용성/세대변경에만발행, ServerFacing은같은assembly재사용용internalstatic. 세대바뀐뒤현재모델delegate재연결/해제정확성확인.

검증목표: 동결코드/아트·컴파일·정적리뷰/의미있는fixture 후 실제UI로지정계정1로그인→캐릭터5011선택/Enter→Zone/ownFull→C03실제지면클릭/좌표/입력차단→C04실제Full/Delta와표시대조. 지정계정2/기존캐릭터5012의별도ClientCore 정상로그인/진입/정상종료로타인표시/TTL확인가능(두번째것은UI통과근거아님). 기존전용계정/캐릭터만사용,DB생성/재시딩/타인접속/강제종료금지. 보호영역필요시정확후보후UnityCoordinator앱Slack승인,현재최소안은필수서버승인없음.

비용/종료: 관련C02/C03/C04 fixture1회, 공통실UI로그인/진입120초+이동/관찰/다른클라이언트종료/TTL확인추가최대5분, 캡처PNG+실제짧은GIF약1분. 필요한조건통과후반복금지. 실패/수정후영향범위만재검증,같은원인2회수정실패면재현·로그root인계. 실제UI성공과합성fixture/Editorpreview/TTL추정숨김을구분한다. 마지막Play/도우미/Recorder/추가클라이언트정상정리·기존서비스유지.

Slack 변경 스레드: Unity Coordinator 앱 C04 /1792310556.105339. 계약사전조사는완료됐으며이번사용자지시로제품구현단계가승인됐다.

초기리뷰3 P2 수정단계: 파괴된visual Unity-null guard후SetActive/Destroy·dict항상정리, MonsterDelta per-ID(state/invalid/deathSeen)원자검증뒤HP0처리/동일packetDelta부활금지, CoreViewModel4 Full/Delta enqueue시subscriber capture+기존lifetimeguard. player2메서드도C04세대정확성에필요한동일좁은호환범위로추가수용했다(새서버/wire/API/인증정책변경없음). C04fixture9개로보강,실제queuedoldcallback→close/open→drain과파괴순서를포함. 현재수정source/DLL동결·재리뷰중이며actualfixture/입력은아직실행하지않았다.

별도테스트준비 소유권: c04_tester는 C04LiveBatchUiTests.cs/meta, Tools/C04Peer의bounded.NET9console테스트도구와Captures결과만작성한다. ClientSession이static이므로두번째계정은별도프로세스로격리하며현재DLL참조/기존Newtonsoft만사용한다. peer는fixedunity_ai_02·기존캐릭터5012만정상로그인/진입/Dispose;DPAPI자격은메모리만/암호·토큰출력없음. READY/EXIT/비밀없는FAIL제어기록외대량snapshotJSON수집을하지않는다. 실행전product·helper재리뷰후최종hash/compile동결,Editor는테스트담당단독.

최종제품3 P2 수정재리뷰: C04_REVIEW.md 코드상차단문제없음. componentD774.../CoreVM1D180.../DLLF541.../9fixture253D...동결,아트BuilderD375.../Scene77F58...보존. 현재c04_tester가새UIhelper/별도peer를준비중이며helper리뷰전실Play/fixture/입력/로그인을시작하지않는다. source수정역할은모두동결상태이고Editor권은테스트담당만갖는다.

## C04 최종 검증 — 2026-10-18

구현·독립 리뷰·관련 PlayMode22/22·실제 B01/B02/B03 입력 배치·PNG/실제 GIF를 완료했다. C04는 기존 Full/Delta로 타 플레이어 및 몬스터0/1/2를 표시한다. 실제 타 플레이어/몬스터 상태·좌표·방향 대조, 근/원/대각/교체 이동, HUD 입력차단, disconnect/재진입, 정상 peer 종료 뒤3초 관측만료, 중앙 AOI type0/1/2 및 최종정리를 통과했다. 근거: Captures/c04-batch-final.txt / c04-fixture-results.txt / c04-phase1-pass.txt / c04-center-types.txt / c04-recorder/c04-server-live.gif. Coordinator가 PNG·GIF디코드 프레임을 직접 검수했다.

HP0 실전투 미관찰과 존재하지 않는 월드 텍스트 입력란 focus 미검증은 별개다. 지면 아트는 대표 구역만 덮어 중앙 캡처에 외곽이 노출된다. 초기 실행/도구 실패와 최소 보정·재리뷰 이력은 보존했다. 원본 서버·DB 추가 변경/커밋/push/공개 없음. 최종 Play/compile/background/runner=false, Recorder/테스트peer/게임연결0, 기존 서비스와 원본Game 변경 보존. 최초 잔여 peer도 전용 콘솔 정상 입력으로 exit0 정리했다. 사후 테스트peer reader 비동기 대기 보강은 build/정적 검토만이며 실제 배치 사용본과 구분한다.
