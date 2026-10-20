# PR37 리뷰 수정의 미게시 catalog 이식

PR37 대상 리뷰 수정은 codex/pr37-review-fixes의 332fbabac857e53bd1b3285b8b1b2114b208a659로 독립 보존했다. PR37 실제 head eacb744에서 분기했으므로 아직 승인되지 않은 DATA01 전체 편집 확장을 섞지 않았다. 게시 요청 PR37FIX01은 Slack parent1792460883.167359/request1792461166.269559이며 아직 사람 원문 승인 전이다.

같은 오류가 남지 않도록 codex/content-catalog에도 공통 C14ExportPair(helper와IO테스트 byte동일), respawn0/비최종 절삭HP threshold>=2 검사, C++ pending guard를 이식했다. 모든기존몬스터를ValidateMonster에서검사한다. 기존asset/runtime확장타입은보존했고Editor통합은catalog버전으로복원했다. 실제asset파일hash는검증전후일치한다.

독립이식검토차단0, CatalogExport2/2PASS. PR버전3/3및공통OSI/O1을반복하지않고별도전체목록의출력/검증경로2개만확인했다. 정상기본표·수치·스크린샷은바뀌지않아재export/사이트·게임입력/서버DB작업0. UnityPlay/compile/Scenedirtyfalse. Captures/pr37-review-catalog-tests-final.txt.

이번코드리뷰요청은일반개발재개가아니다. DATA01확장은계속미게시상태이며기존승인요청에이후수정을숨겨포함하지않는다. 향후게시시현재수정된후보와PR37FIX01반영상태를대조한다. 기존ea01991단독버전의결함을다시올리지않는다.