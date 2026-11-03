# 웹 보완과 남은 촬영

## 반영

- 비교표를 Unity 클라이언트 기준의 기존 기능 / 새 기능으로 정리했다.
- 활용 기록의 두 시안은 동일한 16:9 미리보기, PC300px·모바일260px 이내로 맞췄다. 전체 시안은 같은 페이지에서 펼친다.
- 제목·설명·지표의 크기와 굵기를 구분했다. 기존 워크플로우 A는 유지했다.
- 기존 실제 촬영의 보상·미니맵 영역을 확대해 GIF2개를 추가했다. 기존8개와 합계10개다. 새 플레이를 촬영한 영상으로 설명하지 않는다.

## 근거와 확인

원본은 ROOT/UnityClient/Builds/ContentTourCapture-2026-11-02/frames의 skill-cleave·skill-strike take다. 전체8초의 순서·시간을 유지하며 영역만 크롭·확대했다. site/media/releases/2026-11-03/content-tour-media.json에 원본 크기·프레임·시간·편집 범위를 기록했다.

포션 HUD의 초기 수량 표시는 불명확하므로 공개 설명에서 ‘7→8’을 제거했다. 경험치160→170과 HP포션 획득·수량 갱신만 설명한다. 생성 HTML·실제 캡처에도 동일 문구를 반영하고 종합 검수 담당이 수정본 통과/필수0을 확인했다.

엄격 빌드, PC1265px·모바일375px 넘침0, 시안 펼침, 추가 GIF실제 로드1000px/560px, 본문 링크0을 확인했다. 기존8개 필수 검사와 새로 추가된 모든 GIF·포스터 실재 검사를 통과했다. 실행 지표564회는 이후 갱신·표 합계를 확인한다.

## 미완료

새 몬스터 접근·교전·처치와 HP포션 사용 촬영은 아직 하지 않았다. Unity CLI최신beta15는 확인했지만 연결된 Editor가 없고, Windows앱 관찰에서 다른 게임이 실행 중이었다. 입력 시점을 비동기로 질문했으며 미수신 답변을 승인으로 간주하지 않는다. 이 두 항목은 새 촬영이 가능할 때 이어서 진행한다.

새 게임·서버 실행이나 코드 변경은 없었다. 브라우저 미리보기는 기존8794 서버가 종료돼 있어 새 소유 프로세스로 복구했다. Python 미리보기는 사용자 컨텍스트에서 숨김 실행했고 preview-state.json에 현재 소유자를 기록했다. 이전 잘못된 컨텍스트의 미리보기는 Ctrl+C로 중단했다.

브라우저 증거는 같은 촬영 폴더의 usage-preview-2026-11-03-desktop.jpg/mobile.jpg, usage-full-2026-11-03-desktop.jpg, rewards-2026-11-03-desktop-r2.jpg다. 기존 후보 게시 승인을 이번 추가 diff에 확대 적용하지 않는다.


## 후속 촬영 완료 — 2026-11-03

사용자가 Unity 입력을 허용하고 test1 로그인을 완료한 뒤 동일 Play 세션을 유지했다. 캐릭터11/char_1로 Zone2에 정상 진입했다. 게임·서버·사용자 몬스터 AI 코드는 변경하지 않았다. 서버 소스는 NetLibValidation/source의 c4c509e이며 기존 검증 빌드 MainServer를 포트10099로 정상 기동했다.

GameView.Focus 후 Application.isFocused=true와 프레임 진행을 확인했다. 기존 LoginEditorInputRun에 Assets 밖 임시 IEnumerator를 전달해 QueueStateEvent→정상 PlayerLoop로 UI 클릭·키 입력을 처리했다. Pipeline의 강제 InputSystem.Update를 쓰지 않았고 게임 핸들러를 직접 호출하거나 상태를 주입하지 않았다. 물리 키 검증과 구분한다.

- monster-encounter.gif: 새 monster-finish 원본71프레임/9.025초. 몬스터 접근·피격·C 내려찍기 처치, EXP180→190와 HP포션 획득8→9가 원본에서 확인됐다. crop380,200,1280,720; 출력1000×578.
- hp-potion-use.gif: 새 원본55프레임/7.036초. 1번 슬롯 사용 후 HP10375→11000, 포션9→8, 재사용2.0초 안내를 확인했다. crop470,510,1280,720; 출력1000×259. 중복 프레임 병합 후47프레임/7.04초이며 전체 시간·순서를 보존했다.
- 두 영상은 별개의 촬영이며 연속 기록이라고 주장하지 않는다. 앞선 monster-combat/monster-encounter take도 원본으로 보존하고 최종 채택 영상과 구분한다.

총 GIF12개. manifest는 원본 크기·crop·순서·시간·편집 범위를 기록했다. 엄격 사이트 빌드와 새 GIF 실제 로드, PC1280px/모바일390px 넘침0, 펼침을 확인했다. 화면 근거는 새 촬영 폴더 encounter-desktop.png/potion-desktop.png/encounter-mobile.png/potion-mobile.png다. 종합 검수 담당은 내용·PC 통과 후 빠진 모바일 증거를 요청했고, 같은 담당자에게 보완한 캡처를 전달해 모바일도 PASS/필수0을 받았다.

게임 연결을 정상 종료하고 Play off, background=false로 복원했다. 기존 활성 autotick은 유지했다. 소유 MainServer는 q+Enter 정상 종료/exit0, 로그인 helper도 LOGIN_STOPPED를 확인했다. DB·Redis를 종료하거나 초기화하지 않았다. 잘못 Assets 아래 저장된 소유 로그인 캡처는 외부 촬영 폴더로 이동하고 해당 meta를 정리했다. 검토용 웹 preview는 유지한다.

실제 Agent 추가 신규1·후속4를 반영해 전체569회(새220/후속349). 기존 승인 CONTENTTOUR02는 대체되었으며 현재 diff 게시에는 재사용하지 않는다.
