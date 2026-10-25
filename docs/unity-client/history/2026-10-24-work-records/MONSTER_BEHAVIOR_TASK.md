# 몬스터 행동·이동 템포 묶음

사용자 재개 요청에 따라 codex/classic-mmorpg-motion 2f99cb0 위에서 관련 플레이 흐름을 통합한다. 원본Game/WIP·미게시 거래소42dc·catalog848 보존. 이번 묶음은 지형 확대/Cell 재구성 전에 기존 Cell 안의 자연스러운 행동과 속도를 고친다.

- 서버: WORLD_MOVEMENT_SCALE=.25, 플레이어예산.25unit/50ms=5unit/s. 기존 CAP3는1unit요청 구형 클라이언트를 치트로분류하지않도록유지한다. 초과요청은기존Move경로로예산클램프. wire/DB/계정·치트임계·HP/보상불변.
- NPC: 명목 데이터 속도에 같은 배율을 적용해 추적5/6/3unit/s, 배회3unit/s와1~3초구간/0.5~1.5초멈춤. 개체별결정적seed로방향선택, spawn±6과현재Cell을넘지않고 멀어지면귀환. 살아있는aggro시정확1회증가·5초기존timeout동작, missingtarget초기화. 보스는스킬tick으로발동간격계산. 리스폰상태·dirty정리.
- 클라이언트: 요청.25/50ms와화면예측5unit/s정렬. I가방/B거래소는기존TogglePanel경로를사용하고포커스/텍스트입력중차단. 새패널/아이템아이콘전면통합은후속작업.
- 소유권: motion_remote=서버AI및native2시나리오·friendfixture, motion_contract=읽기분석/독립리뷰, Coordinator=Config·클라이언트속도/I-B·통합검증·문서/Git. classic_item_art=Assets밖HP포션1개시안. 테스트동안Assets동결/Editor단독.
- 최소검증: 서버ReleaseMainServer+UnitTests빌드, 실제ZoneState의배회/재생성1개와타이머·추적속도·Cell경계·보스간격1개, 영향C16자기캐릭터시각속도1개. 기존전체회귀/로그인반복없음. I/B실제입력·새AI실서버사냥은기존이동배치와하나로합쳐대기한다.
- 배포: 현재9999의구매기능후보서버23696은교체하지않는다. 새후보는보존된거래소기능을되돌리지않는통합버전준비후정상전환한다. 운영/DB파괴/강제종료없음. PR은큰게임흐름묶음으로합치고게시직전Slack승인.