# 현재 인계 — 2026-10-24

## 현재 목표·정본

사용자 ‘작업 재개’로 Unity 개발을 재개했다. 현재는 이동 정지 보완 완료·기사 재질 비교안 선택 대기다. 몬스터 AI는 사용자 담당이며 변경하지 않는다.

- Git 정본: C:/Users/joeun/Desktop/workspace/Game-UnityClient/UnityClient/Builds/C10-Delivery/source, codex/client-stop-reconciliation. 문서 정리 로컬 커밋도 보존한다.
- 실행 Editor: C:/Users/joeun/Desktop/workspace/Game-UnityClient/UnityClient,6000.0.47f1/Pipeline7800. 두 변경 소스가 정본과 일치한다. 마지막 관찰 Playfalse/Scene clean/backgroundfalse.
- 원본 Game은 사용자 서버·AI/문서 변경을 보존한다. 이번 서버·DB·프로토콜·속도 변경0, 새 서버 실행·실게임 접속0.
- PR51은 병합 완료. 현재 묶음은 로컬 후보이며 push·새 PR·배포는 하지 않았다. 작은 수정 PR로 즉시 쪼개지 않는다.

## 완료와 제한

키를 뗀 뒤 미확정 시각 오차를 정리하는 유예를100→350ms로 맞췄다. 컴파일과 새200ms지연/무응답 회귀2개 통과. 실제 지연 네트워크의 조작감 통과를 뜻하지 않으며, 미확정 충돌 오차가 최대250ms 더 유지되는 절충이 있다. 상세는 [이번 결과](history/2026-10-24-client-playability/RESULTS.txt).

## 사람의 선택과 다음 행동

기사 갑옷 A밝은강철/B절제된강철/C무광흑철의 같은 조건 에디터 시안을 제시했다. 사용자 답변 전 B 추천을 확정 취향으로 기록하지 않는다. 모델/텍스처/조명/나머지 재질은 고정했다. 비교 파일은 ArtSource/knight-material-options와 ROOT UnityClient/Builds/Client-Resume/ArtOptions다. 선택하면 적용 범위·임시/확정을 BRIEF에 남긴 뒤 대표 화면에만 적용한다. 원본 에셋은 아직 그대로다.

Unity 실제 입력의 마지막 사용자 허용은 유지하되 이번 회귀는 실입력 검증이 아니다. 새 입력 전 대상 창·사용 상태·서버/DLL을 확인한다. 과거 승인·PID를 현재 상태 근거로 쓰지 않는다.

실서버 판매 등록·실재화 차감·아이템이 든 가방은 기존 데이터 조건상 미확인으로 남긴다. 다양한 지연·모든 몬스터 특수 공격·전체 구역 실전도 완료로 바꾸지 않는다. 이미 통과한 검증은 반복하지 않는다. 게시 직전에는 준비된 큰 후보에 대해 기존 Slack 승인 절차를 따른다.

문서 분류와 과거 기록은 [문서 안내](README.md)에서 찾는다. Agent 실제 누적280이며 총괄의 직접 검증·렌더는 호출 수에서 제외했다.
