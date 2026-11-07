# 최신 실행 후보 패키지 — 2026-11-07

## 범위와 출처
- PR73 d98141f의 추적 Assets/Packages/ProjectSettings 1859파일을 ROOT/UnityClient/Builds/ItemUsabilityPackage/Project로복사했다. ROOT편집본은덮어쓰지않았다. 최초입력해시manifest보존,Library/Builds/캐시복제없음.
- 원편집본Assets1832대조시추가파일0·서식외6입력차이를발견했다. 재질legacyalias3·URP생성설정2·동적TMPfallback1은패키지자동처리와대응하지만출처를추정하지않고별도정본복제로빌드하는대안을Astra가선택했다.
- 초기UPM해결약119초후완료. 버전하한관련경고는있었지만manifest/lock의JSON값/실제해결버전은정본과같았다. 의도적패키지업데이트없음.

## 확인 결과
- Windows64 Player build_f463960a42d8 성공,오류0·경고5·366793ms. 대상World씬1개,Windowed1280×720. 실제게임기동아님.
- 경고5개는PlayerRuntimePipelineConfig없음1(런타임자동화브리지비활성) 및 기존TMP enableWordWrapping obsolete4. 이번패키징을위해런타임제어권한을새로열거나관계없는UI코드를고치지않았다.
- Player에ClientCore.dll·Mmorpg.Client.dll포함,테스트어셈블리제외. ClientCore원본/PlayerSHA256일치. Player/UnityPlayer/ServerAMD64·필수데이터/Mono런타임존재확인.
- 별도사본의World씬4725개체누락MonoBehaviour0,InventoryScreen1/MarketScreen1. Read-only편집씬확인이며Play/실입력/네트워크실행아님.
- 빌드후달라진입력은URP프리필터·runtime settings·동적TMPatlas·씬/그래픽서식·InputSystempreload. Source씬/게임코드/아이템GUID/9종수치는유지된다. InputSystem의기존설정GUID052faaac586de48259a63d0c4782560b를BuildProviderHelpers가preload추가하는경로와대응하며이파생설정을Git정본에복사하지않는다.

## 서버와 복구 기록
- 기존통과한MainServerReleaseA6149632BC31A347E82533577C430DDE65288839EB92A239F167A149C547561C를새Server폴더에복사했다. 서버데이터/프로토콜/DB/AI추가변경없음.
- native의존성1차검사는app-local/System32만보아OpenSSL2개가외부인것을발견했다. 기존기동실패로단정하지않았다. 실제MachinePATH에는설치MySQL8.4/bin이있으며도구PATH에는별도PopplerDLL도존재했다.
- 대안검수후현재설치된MySQL8.4의OpenSSL3.0.15쌍만새비공개후보에복사했다. 원본/시스템설치/PATH변경·다운로드없음. 9개native파일AMD64·재귀의존성과app-local import/export756개·복사해시를확인했다. 정적의존성충족이며기동/DB/TLS호환성성공은미확인이다.
- 기존DefensiveGear/Server의backpressure.1(0bytes)·redo.8(4396bytes)을프로세스정지확인후복사하고해시를대조했다. 원본보존,재생/삭제/초기화없음. 런처전환전에원본파일집합/해시와정지상태를다시확인한다.

## 다음 결정과 한계
- 실제기동·로그인·기본/등급포션소비·조작감은사용자부재중미실행. DB지급·상점/드롭/자동공급추가없음. 기존기능검사재실행없음.
- 후보위치 ROOT/UnityClient/Builds/LocalGame/TierPotions. 상세입력/빌드/파일해시/의존성/복구manifest는ROOT/UnityClient/Builds/ItemUsabilityPackage에비공개보존.
- 런처대안:A기존경로유지, B새쌍경로로전환하되기존WAL/프로세스정지재확인·설정해시보존/원자교체·오프라인check만수행. B추천제안이며아직적용전이다. 게임/서버를시작하지않는다.
- DoNotShip디버그폴더1개는증거폴더로이동해보존하는안을제안한다. 기존DefensiveGear·ROOT편집자산·사용자변경은보존한다.

## 최종 적용 범위
- Astra오프라인패키지검수통과/필수0. 런처B전환과디버그폴더보존이동은조건부계획통과였으나,두작업을포함한명령자체가자동승인심사에서거절돼실행되지않았다.
- 거절사유:기본실행환경을새Player/Server경로로교체하는정확한사용자승인없음,패키지보존이더안전한대안. 우회/간접적용하지않았다. 설정SHA가전환제안전과같고DefensiveGear경로유지,디버그폴더원위치,전환결과파일없음확인.
- 검증용별도Editor13268정상종료. 게임/서버기동없음. RUNPAIR01/r1 요청1794031948.946109를UnityCoordinator동일스레드에남겼다. 승인은도구심사를대체하지않는다.
- 문서검토에서포션ID오표기(HP6/7→6/8,MP8/9→7/9)와누락된format4거부/방어율제약설명을실제정본으로교정했다. 기록Agent의ROOT4개문서부수수정은작은절/문구교체임을확인했고,관련현재사실교정만부모검토로채택하며나머지ROOT본문은보존했다.
- 원문기록은보존하며현재절/PR상태만정정했다. 이번2시간세션의강화계획검수는영구지침이력에추가하지않았다.
