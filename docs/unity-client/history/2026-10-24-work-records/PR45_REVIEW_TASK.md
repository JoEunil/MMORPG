# PR45 리뷰 보완

두 리뷰는 현재 PR45 단독 기준으로 유효하다. 경계 입력은 PR46의 검증된 수정이 선행 PR45에 없던 문제이며 같은 C02 입력 목표/fixture만 이식한다. 서버 전환 임계값·중첩 폭·권위 좌표 검증은 바꾸지 않는다.

DummyClients는 legacy ACTION18/ZoneACK31을 사용해 새 ACTION26/ZoneACK47과 불일치한다. Dummy 도구 안에서 두 builder·epoch/state/readiness/정확한 프레임 길이 검증·authoritative 좌표 반영을 함께 맞춘다. 서버의 새 wire를 완화하거나 legacy 동시 허용을 추가하지 않는다. 새 몬스터 AI/A*와 사용자 원본 Game 변경은 제외한다.

검증은 수정 동결/독립 리뷰 뒤 DummyClients 빌드와 네트워크 없는 protocol self-test 3개 이내로 끝낸다. 기존 경계 fixture와 실제 Unity 1→2→1 검증은 같은 수정의 확인된 근거를 재사용하고 반복 Play/입력·서버 재시작·5000계정 부하/DB 조작은 하지 않는다. 오프라인 self-test는 실제 부하 성공으로 보고하지 않는다.

구현: dummy_epoch_fix는 CAND DummyClients 전용. map_server_finish_audit는 읽기 계약 분석. Coordinator는 경계 port·기록·최종 리뷰/최소 실행·PR45/PR46 통합을 담당한다. 공개 화면 변화가 없는 호환 수정이므로 새 게임 캡처·공개 HTML 변경을 만들지 않는다.