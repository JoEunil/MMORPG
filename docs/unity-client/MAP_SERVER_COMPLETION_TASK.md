# Map Exporter·서버 연결 마무리

2026-10-22 사용자 목표: Map Exporter와 관련 서버 적용까지 완료한다. 기존 6구역·AOI25·별도 navigation grid를 유지하며, 사용자 담당 몬스터 AI/A*와 Cell 이동 제한은 변경하지 않는다.

이미 완료한 exporter 생성물/실패 보존·6/9 예시 컴파일·서버 이동/소유권/epoch/첫 Full 쌍 자동 검증과 마을 실제 귀환을 반복하지 않는다. 이 묶음은 남은 살아있는 캐릭터의 벽 충돌과 자동 경계 전환을 끝내는 데 집중한다. Additive Scene streaming/이웃 Zone 합동 AOI나 9구역 활성화 완료로 확대 해석하지 않는다.

읽기 감사에서 generated WorldMap.inc가 실제 서버 이동 충돌과 owner 전환에 연결됐음을 확인했다. 다만 클라이언트가 owner 경계205를 넘는 입력 목표205.1을 취소해 전환이 막힐 수 있다. 로컬 목표에만 기존 최대 lead0.6의 여유를 허용하고, 서버 응답/이동 원점·충돌/속도 검증은 유지한다. 서버/wire 변경이나 재시작은 필요하지 않다.

소유권: map_server_finish_audit는 좁은 연결 분석, map_border_input_fix는 C02WorldScreen/C16OwnMotionTests, town_switch_review는 읽기 독립 리뷰. Coordinator가 Scene/Editor 입력·통합·문서·공개 캡처를 담당한다. ROOT/Game 서버 WIP 보존, ROOT Unity는 두 파일 hunk만 baseline 검사/백업 후 동기화한다.

최신 사용자 답변에 따라 Unity 실제·모의 키 입력은 보류한다. 서버 검증은 기존 C04Peer의 --map-check로 공개 UpdateMove를 전송하고 실제 Full/Delta·epoch를 관찰한다. 직접 DB 수정/서버 내부 상태 주입은 하지 않는다.

추후 입력 재개 시 검증: 신규 경계 fixture1(경계 목표 허용/lead 유지/비정상 Full 거부), 살아있는 전용02 한 접속에서 키 입력으로 실제 벽 접근·통행 차단과 1→2→1 자동 전환/첫 상태·조작 복귀를 확인한다. 필요하면 같은 벽 위치에서 직접 네트워크 요청으로 서버 충돌 거부를 관찰하되 이를 UI 입력 성공으로 세지 않는다. 기존 로그인/시장/전투 전체 회귀는 제외한다. 실패는 보존하고 영향만 고친다.

서버 완료 조건: 실제 생성물과 실행 서버 연결, 장애물 통과 방지, 좌표 연속 owner 전환/첫 Full 준비, A* 연결 문서 갱신. UI 입력은 별도 미실행으로 남기며 사용자 보류 중 새 캡처를 만들지 않는다. 기존 큰 맵/마을 PR 흐름에 합치며 작은 PR을 새로 만들지 않는다. 게시 범위와 기존 승인을 대조하고 추가 승인 필요 시 Unity Coordinator 앱으로 정확한 후보만 요청한다.
