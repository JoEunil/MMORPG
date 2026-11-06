# 방패·갑옷 방어 효과 — 구현·검증 결과

## 범위와 승인
2026-11-06 사용자 비동기 call_5a9849c6fd6f4578b970b993a9e0085b의 ‘추천안: 합계5% 감소로 구현’을 적용했다. 방패4=2%, 몸통5=3%, 각1칸, 보유품만, 수량 유지, 장착ID 모든 스택 차감/판매 차단, 타격 시점 판정, 재접속 유지. 기존 철검+2·외형·패링·몬스터 AI는 유지한다. 새 inventory/WAL 형식과 구 서버 직접 복귀 제한도 같은 질문에서 고지했다. 직접 DB 지급·스키마 변경·드랍·상점·제작은 하지 않았다.

정본은 HANDOFF의 canonical, codex/defensive-equipment, 기반68c63f3(PR70 병합), 선행 로컬776fb65의 거래 문구 보완도 같은 큰 묶음에 둔다. 아직 push/새 PR 없음.

## 최종 구현
- 가방 권위의 무기·방패·몸통 상태와 서버 진입 선적재·구역 전체 상태 복사·조회/변경·ClientCore 수명·가방 표시를 연결했다. 무기전용 API는 실제3슬롯 책임의 EquipmentStateResult/RequestEquipItem로 바꾸고 별도 Manager/wrapper는 추가하지 않았다.
- 서버 ItemData 정본 방어율을 JSON5 items.defensePercent로 전달한다. 각 수치와 합산0..99%를 명시 검증하고 잘못된 데이터를99%로 조용히 대체하지 않는다. 기존 GUID e20a847d2d4fa094088b01f4c513165b 보존.
- 몬스터 피해 계산은 uint64 곱·기존 치명타→특수 패링→방어율 내림→HP 차감 순서. 원피해0/패링0/나머지 양수 최소1, 매 타격 현재 장착 상태. 분할 곱/나눗셈으로 UINT64_MAX에서도 overflow 없이 내림한다. AI/이동/phase/공격계수는 변경하지 않았다.
- wire53 GET24 불변,54 SET29,55 RES46. slot·요청ID·epoch·operation·status·장비ID·실패zero·공격합 포화·방어율을 대조한다. 정상 SET 재시도/늦은 응답/미확정 결과에서 임의 장착·판매를 허용하지 않는다. DummyClients는 장착 요청을 보내지 않아 기존 enum/행동 패킷은 영향 없다.
- inventory376=기존360 prefix+IEQ2/weapon/shield/body, WAL3 payload384. 지원84/352/360·zero2404·IEQ1 368 및 WAL1/2/3의 명시 해석과 기존 다중스택을 보존한다.

## 확인한 결과
근거 경로: ROOT/UnityClient/Builds/ClientGrowthHUD/verification/defensive-equipment.

| 확인 | 결과·근거 |
| --- | --- |
| 빌드 | ClientCore 오류0/경고0, Unity 오류0/경고0, MainServer·UnitTests Release 성공(server-build-final.log). Windows Player Succeeded/오류0/기존경고5(player-build.json). |
| 서버 피해·상태/저장 | 기존 Core2/Cache2 검사 전부 통과(server-equipment-tests.xml). 무장착/단독/합산/0/최소1/넓은값/패링/치명타순서·소유/slot/epoch/dedup·형식/WAL 호환·실패 무변경 포함. |
| SDK·가방 | 기존 Equipment2 검사 전부 통과(unity-equipment-tests.json). 46byte·slot/ID·상관성·닫힌창·늦은응답·판매보호. |
| 콘텐츠 | 기존 import2 검사 전부 통과(content-tests.json). JSON5/GUID/필수필드/합산범위/old4 거부·잘못된 입력의 기존asset보존. |
| 정상 거래 | test1의 기존 철방패 매물을200다이아에 구매·수령, 다이아650→450/골드100020 유지, 방패1개. shield-purchased.jpg. |
| 장착·판매 | 방패 단독DEF2%/철검 동시ATK11+2=13·DEF2%, 수량각1. 장착 방패 선택 시 판매버튼 차단. shield-equipped.jpg/weapon-shield-equipped.jpg/shield-sale-blocked.jpg. |
| 완전 재시작 | 같은 후보의 게임·MainServer·Login 정상 종료 후 재시작·재인증·월드진입. 철검/방패On·On·Off,ATK13/DEF2%·수량1/잔액450 복원. shield-after-server-restart.jpg. |
| 해제·종료 | 방패 해제DEF0%/무기 해제ATK11로 각각 복귀·수량 유지. shield-unequipped.jpg/all-slots-reset.jpg. 게임·MainServer·Login 정상 종료 완료. |

자동 검사는 직접 바뀐 기존8개만 실행했고 중복 재실행하지 않았다. 선행 거래 안내 검사1개는 이전776fb65에서 통과했으며 이번에 반복하지 않았다. SDK/UI/data와storage/Core의 독립 코드리뷰는 중요결함0이다.

## 한계·환경
- 지정 캐릭터11/5011/5012의 갑옷 보유0, 기존 거래소 갑옷 매물 없음. 정상 공급만 재사용하므로 갑옷 실전 장착·실제 전투 피해·사냥 체감·구역 왕복은 이번 배치에서 미실행이다. 갑옷3%·합산5%·실제 ApplyHit 경로는 자동fixture로 확인한 것이며 실전 성공으로 바꾸어 표현하지 않는다.
- 실제 입력 전 다른 작업 창을 확인해 사용자 call_f29fa15edc794c6687d7011c56209d04 허용 후 진행했다. 도중 사용자 입력 감지로 멈추고 call_e50b3fd2dea14953a7583d3fcc04de66 재개 답변 후 남은 검증을 수행했다. 다른 창에는 입력하지 않았다.
- 초기 /m2 빌드는 worker 통신 대기로 Ctrl+C 정상 취소했다. /m1/nr:false로 실제 빌드 성공. 권한/보안 설정을 바꾸지 않았다. 기존 C4244/C4267·/arch:SSE 경고와 Unity runtime pipeline/TMP 경고는 성공으로 숨기지 않고 빌드 근거에 보존했다.
- 현재 런처는 DefensiveGear/Server와Player 쌍이다. MainServer SHA256 5DAB2A43B6E4035086EBD26CBF6302674F1AA0773B5377D1E629CB359C0BBB62. 이전settings-before.json과Equipment 산출물은 보존하지만 이전 서버를 바로 실행하면 안 된다.

## 복구 조건 — 실행하지 않음
모든 슬롯 해제→정상 flush/종료→DB·WAL 백업→새 WAL3 재생/flush 완료 확인→승인된 오프라인 DecodeInventory/EncodeLegacyInventory 변환(장착 중에는 거부)·수량/LSN 대조→호환 서버·클라 쌍 전환 순서다. old368/360로의 직접 재시작·신 WAL 삭제는 하지 않는다. 실제 DB 변환·구 버전 복구 성공을 수행하거나 주장하지 않았다.

## 종합 검수
Astra는 실제6장을 직접 열어 장착/판매보호/재시작복원/해제 및 공개 설명의 실전·자동 검사 구분을 확인했다. 게시검수 통과·필수수정0. 전체 실전 완료로 보고하지 않으며 남은 실제 갑옷/전투/구역/구형복구 한계를 유지한다. 슬롯별 On/Off 대응을 더 명확히 하는 것은 선택 개선으로 남겼고 검수 후 제품 변경은 하지 않았다. 사이트 빌드·링크 검사도 통과했다.
