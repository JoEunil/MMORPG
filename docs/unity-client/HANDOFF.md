# 현재 인계 — 아이템 정본 공유·기본 무기 장착

## 범위와 정본
- 2026-11-06 사용자 승인: 보유 철검 1칸 장착, 공격력 +2, 가방 수량 유지, 같은 철검 판매 전 해제, 구역·재접속 유지, 자동 지급 없음. 피해는 타격 시점 장착 기준. 저장 형식 확장과 구 서버 직접 복귀 제한도 승인했다.
- ROOT: C:/Users/joeun/Desktop/workspace/Game-UnityClient. Git 정본: ROOT/UnityClient/Builds/ClientGrowthHUD/source. 실제 Editor: ROOT/UnityClient.
- codex/item-equipment, origin/master f205bf16(PR69 병합)에서 분기. metadata 로컬 커밋 d016407 이후 장착 후보 구현. 사용자 Game/refactor·몬스터 AI는 수정 금지. 포션 등급·드랍·일반 우편은 제외.

## 완료한 구현·검증
- 서버 itemMap → JSON format4 items → 기존 Unity importer/UI. 서버 이름·효과·무기 공격 보너스를 공유하며 기존 아이콘·GUID 보존.
- 가방 권위의 장착·WAL 선확정·동일 아이템 모든 스택 차감 차단·월드 진입 선적재·ClientCore 응답 상관성·가방/거래소 연결 구현. 기본 공격력은 덮지 않는다.
- ClientCore·MainServer·Unity 컴파일 및 Windows Player 빌드 성공. 직접 자동 검사 8개 통과(C++4/장착2/import2). 독립 코드 리뷰 차단0. 실제 DB 5,002행 읽기 전용 호환 검사 통과.
- 실제 test1: 100다이아 철검 구매·수령, 수량1 유지, 장착 공격력 11+2=13, 판매 차단, 재접속 후 유지, 해제 후11 확인. 레벨4의 기존 기본값11이며 신규 기본10→12 정책과 구분한다.
- 게임·MainServer·Login 정상 종료 완료. runtime 설정은 Equipment 빌드를 유지한다. 실제 구역 왕복·전투 피해 측정·서버 재기동 후 복원·구형 변환 복구는 미실행이다.

## 제한·다음 행동
- DB 스키마/직접 DB 수정 없음. 정상 거래로 다이아750→650, 철검0→1. 장착은 해제해 종료했다.
- 기존84/352/360 및 전체zero2404를 명시 해석하고 새 저장은368, WAL type2. 구 서버를 바로 시작하지 않는다. 상세 형식·복구 조건·증거는 [결과 기록](history/2026-11-06-item-equipment/RESULTS.md).
- 실제 입력은 이번 검증에 허용됐으며 배치 종료. 이미 통과한 검사는 반복하지 않는다.
- Astra 제품 검수 통과, 최종 기록 보정도 반영. 사이트 빌드도 성공. 현재 채팅의 사용자 승인으로 ITEMGEAR01/r1 게시 범위를 확인했다. 승인된 제품 후보0868b75와 인계 문서 커밋만 일반 push 후 큰 PR1개로 게시한다. 병합은 승인 범위에 포함하지 않는다.

## Slack·지표
- Unity Coordinator bot 부모1793938992.023029. ITEMGEAR01/r1: 현재 채팅의 사용자 ‘승인’을 codex/item-equipment 전체 묶음 push/새 PR1개 게시에 적용했다. 요청1793941901.703789, 지정 사람U0C9QEUHPMK·부모1793938992.023029·현재대기와 대조 후1회만 적용한다. 수신 프로세스43884/listening·원본UTC기준1.2초 health·스레드등록 확인. Slack 회신을 받았다고 주장하지 않으며, 현재 채팅 승인으로1회 적용한다. 게시 이후 같은 승인 재사용 금지.  과거 TRADEITEM01/r1은 PR69에 소비 완료다.
- 현재 실제 활성화 누적677(신규254/후속423): 마지막 Astra 게시검수 후속1 포함. 총괄 직접 구현·검사·입력·도구 호출 제외, 토큰/비용 N/A.
