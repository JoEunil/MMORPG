# Unity 맵 Exporter와 서버 주도 구역 전환

사용자 최신 승인: 첨부 Phase1 다음 서버 전환/패킷 변경까지 이번 작업에 포함한다(채팅 답변 확인). 몬스터 AI/A*/Cell 이동 제한은 사용자 후속 작업이므로 변경하지 않는다. Unity 실제 입력은 계속 보류한다.

현재 3×2/6구역, 글로벌 X/Y, 중첩 ±5, AOI Cell42/5×5이며, 현재 Unity는 Trigger 대신 명시적 구역 버튼 요청이다. nav grid는 이미 분리돼 있으므로 재사용한다. 기존 원점(-5,-5)/610×410/1unit nav를 유지하고 3×3/9구역 레이아웃도 생성·검증할 수 있게 한다. 9구역 데이터 지원과 실행 서버의 9구역 활성화는 구분한다.

Exporter: 각 구역의 expanded 좌측하단을 local0,0으로 정의하고 origin 간격은 core200/영역span210/overlap10으로 기존 월드 좌표와 일치시킨다. 로컬/월드 변환, ownership core/전용·중첩 범위, 8방향 이웃, AOI25구획, 통행bits/clearance/장애물 bounds/format·content version을 한 WorldMap.inc로 출력한다. 기존 순수 nav Export API는 기존 작은 fixture 호환을 위해 유지한다. 에디터에서 레이아웃과 선택 구역 경계/AOI/중첩, 저장된 nav를 확인하고 전체 snapshot을 export한다.

서버: 최종 검증된 이동으로 owner 결정, 현재 owner의 overlap 끝에서 전환하여 hysteresis 유지, 코너는 하나의 대각 목적지. shard→오름차순zone락, 목적지 준비 실패 시 원본 보존, epoch로 stale ACTION/전환 통지 차단. 첫 player/monster Full(빈 목록 포함)을 모두 받은 뒤 화면 교체, 실패 재동기화 경로. 기존 generation과 ownershipEpoch를 혼동하지 않는다. 상세 구현은 worker 독립 경계에서 진행한다.

소유권: layout_export_impl=새C24모델/Exporter·기존C20Export, server_transition_impl=서버/ClientCore/관련 자동검증, Coordinator=Unity authoring/Inspector/어댑터/Scene/통합·게시. Editor 조작자는 Coordinator 한 명. ROOT와 원본 Game WIP는 보존하고 CAND에서 저작한다.

검증: 전체 묶음 동결·독립 리뷰 후 exporter 핵심2시나리오(현6·별도9/경계·장애물·실패시보존)와 서버/ClientCore 직접 영향 최소 시나리오만 한 배치. 기존 로그인/거래소/실입력/AI전체회귀는 제외한다. 실패·미실행을 성공으로 기록하지 않는다. 새 wire/DLL은 구서버와 호환되지 않으므로 실제 배포는 빌드·최소 검증 뒤 묶어서 처리한다. 새 push/PR 직전 Unity Coordinator Slack 승인.
