# 미니맵·마을 성벽·지면 표현 보완

- 사용자 요청: 카메라 방향의 미니맵, 자기 중앙 고정, 배경/개체/N 동시 정렬과 외곽 성벽 연결·잔디 공백 보완.
- 기준: master99505a1(PR73 병합), codex/world-map-presentation. 기존 미게시 인계·테스트 지급 결과를 보존했다. ROOT 편집 자산은 덮어쓰지 않고 동일 씬/맵/스폰/패키지 해시를 확인한 기존 별도 Unity 프로젝트에서 적용했다.
- 미니맵: 카메라 수평 forward/right를 메시당 한 번 계산해 위치/atlas UV 역변환/탐색 격자 fallback/N에 적용했다. 직사각형 틀·거리의 등방 축척·자기 중앙을 유지하며 카메라 pitch로 거리를 압축하지 않는다. 기존 N 라벨을 연결하고 글자는 직립한다.
- 성벽: 외곽11모듈의 작은 틈과 서남 코너2곳 연결. 기존 성문과 길, 내부 장애물18개를 유지했다. 20m cosmetic 지면 apron과 낮은 잔디 메시1개를 기존 재질로 추가했다. 새 통행 영역이나 collider는 만들지 않았다.
- 독립 리뷰에서 apron tangent 누락을 발견해 원아트 담당자가 해당 메시만 보완했다. 재리뷰 필수0. 16정점/16tangent와 실제 collider 직렬화·변환 불변을 확인했다.
- Unity 컴파일 오류0. 직접 영향 WorldMinimap3검사(새 카메라 방향1/기존 직사각클립·수명2) 모두 통과. 다른 게임 검사를 반복하지 않았다.
- 적용 도우미 첫 호출은 엔진 객체에 JsonUtility를 사용해 변경 전에 실패했다. EditorJsonUtility로 수정한 뒤 적용·충돌 불변·N 연결·배경 bake를 확인했다. 명령 인자 오류는 제품 실패나 통과 검사로 세지 않았다.
- 바뀐 지형으로 atlas2048×1418을 다시 bake했다. 실제 씬 미리보기는 corner/road, 카메라 정렬 미니맵은 예시 개체점을 넣은 preview로 구분한다. 새 후보의 실서버·물리 입력 검증은 이번에 수행하지 않았다.
- Windows64 build_df3a8564c596 성공, 오류0·경고5(기존 TMP wrapping4와 Player runtime automation 미설정1). ClientCore DLL은 기존 실행본과 동일하다. 런타임 자동화 권한을 새로 열지 않았다.
- 상세 산출물: 비공개 ROOT/UnityClient/Builds/WorldMapPresentation의 applied.json·minimap-tests.json·player-build-status.json·before/after 이미지·빌드. 서버 코드·AI·패킷·통행/스폰 데이터·DB는 변경하지 않았다.

## 전달 상태
빌드와 코드·화면 검수를 마친 뒤 로컬 Player 교체 및 PR 게시 승인을 별도로 기록한다. 기획으로 보류한 포션 획득 기능은 추가하지 않는다.

- 로컬 Player 교체 완료: 기존 Player 전체를 WorldMapPresentation/PreviousPlayer에 보존하고 배타 런처 잠금 안에서 새169파일을 해시 검증해 교체했다. 기존 시작 경로/포트/서버 실행 파일/복구 로그 불변, offline check 통과. 실제 게임을 새로 시작하지 않았다. 새 화면은 다음 시작 BAT부터 적용된다.


- 최종 종합 검수 통과·필수0. 확인한 코너/도로의 공백 해소와 N/거리 원을 확인했으며, 맵 전체 무공백·실서버 동적 조작감으로 확대하지 않는다. 편집기는 clean/Play off 확인 후 정상 종료했다. Unity 직렬화의 빈 필드 뒤 공백은 의미 없는 포맷으로 구분했다.

