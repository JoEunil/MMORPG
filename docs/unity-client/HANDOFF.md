# 현재 인계 — 미니맵·성벽·잔디 보완

## 정본·환경
- ROOT: C:/Users/joeun/Desktop/workspace/Game-UnityClient. Git 정본: ROOT/UnityClient/Builds/ClientGrowthHUD/source, codex/world-map-presentation. origin/master99505a1(PR73 병합)에서 분기했다. 원 ROOT 편집 자산은 보존했다.
- Editor/검증 사본: ROOT/UnityClient/Builds/ItemUsabilityPackage/Project. 관련 씬·맵·스폰·패키지 해시를 정본과 대조한 뒤 지정4코드 파일·생성 아트·씬만 통합했다. 서버/ClientCore/AI/맵 충돌·스폰 데이터 변경 없음.

## 현재 결과
- 미니맵은 게임 카메라 수평 방향으로 배경·개체·N을 함께 변환하며 자기 중앙·직사각 틀·등거리 눈금을 유지한다. 외곽 성벽11개와 모서리2곳, 성벽 주변 잔디 지면을 보완했다. 내부18장애물·성문·길·Collider 불변 확인.
- 컴파일·미니맵 직접 영향3검사 통과. apron tangent 누락 리뷰를 원아트 담당자가 고치고 동일 리뷰 담당자가 재검토해 필수0. actual-scene 편집 미리보기와 개체 샘플을 넣은 minimap preview를 실서버 화면과 구분한다.
- Windows64 build_df3a8564c596 성공(오류0·기존경고5). ClientCore DLL은 기존과 같다. 기존 Player 전체 백업/169파일 해시 확인 후 로컬 TierPotions/Player만 교체, 시작 경로/서버/WAL 불변·offline check 성공. 게임/서버는 종료 상태이며 새 Player 실서버 로그인·이동 검사는 이번에 하지 않았다.
- 근거: history/2026-11-08-map-presentation/README.md 및 비공개 ROOT/UnityClient/Builds/WorldMapPresentation. 이전 누적 인계는 같은 비공개 증거 폴더의 handoff-before-map-presentation.md에 보존했다.

## 선행 완료·고정 조건
- PR73은 병합 완료. 등급 포션은 사용자 허용으로 지정 테스트 캐릭터에 일회 지급했으며 반복하지 않는다. 이후 중·상급HP/MP4종의 실제 소비·회복·최대치 제한과 공유6초 표시, 만충 미소비를 같은 세션에서 확인하고 정상 종료했다. 상세 계정/수량/지급 영수증은 비공개 ItemUsabilityPackage와 tier-use-live에만 둔다.
- 포션 획득 비교안은 채택하지 않고 보류했다. 드롭은 추후 사용자 작업이며 공급·상점·처치 지급을 이번에 만들지 않는다. 사용자 몬스터 AI 보호.

## 다음 행동
- 최종 종합 검수 통과·필수0. 공개 자료 생성 확인 완료. 로컬 후보를 고정하고 새 Slack 부모에서 MAPVIEW01/r1 PR 게시 승인을 요청한다. 미니맵·성벽·잔디 및 관련 자료를 한 PR로 게시하며 병합하지 않는다. 승인 전 push/새 PR 보류.
- 검증용 Editor는 clean World씬/Play off 확인 후 정상 종료했다. Slack 수신기는 오래된 health/PID 소멸을 확인하고 기존 고정 대상·권한으로 복구했다. 최근 listening·SOCKET_READY·실제 프로세스를 확인했다. 과거 답장은 재처리하지 않았다.
- 현재 별도 PC 입력 요청은 없다. Editor 미리보기/자동 fixture/빌드 완료를 새 standalone 실입력 성공으로 표현하지 않는다. 런처 경로는 그대로여서 다음 실행부터 새 Player를 사용한다.
- 실제 활성화746(신규271/후속475). 공개 AGENT_USAGE는 과거 미반영7회와 이번9회만 더했다. 총괄 작업·도구/검사/입력은 활성화가 아니며 토큰·비용N/A.
