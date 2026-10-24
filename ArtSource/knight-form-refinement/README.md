# 기사 대표 형상 비교 — 2026-10-24

사용자가 선택한 캐릭터·전투 묶음의 대표 구현이다. 기존 ObservedPlayer의 어깨·가슴·팔·다리 형상을 정리했으며 신규 리깅·재질 방향 선택은 없다. 원본 알고리즘 메쉬이며 외부 리소스 구매 없음.

before/after 이미지는 site/media/releases/2026-10-24/knight-form-*.png에 보존한다. 같은 기존 Prefab·고정 카메라·조명·반사·재질 조건의 PreviewRenderUtility 에디터 렌더다. 서버 접속·실입력·실제 전투 성과를 뜻하지 않는다. before는 수정 전 형상, after는 대표 후보이며 미술 만족도는 사람 확인 전이다.

render-preview.cs는 Assets 밖에서 unity run_script로 실행하는 재현용 파일이다. 기본 출력은 after이며 원본 Scene·Prefab·Material은 수정하지 않는다. before를 다시 만들려면 당시 소스/에셋 체크아웃에서 파일명만 before로 지정해야 한다. 이미지를 이름만 바꾸어 이전 상태라 주장하지 않는다.
