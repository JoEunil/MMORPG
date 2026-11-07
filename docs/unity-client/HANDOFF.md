# 현재 인계 — 장비·포션 사용 개선 게시 준비

## 정본·후보
- ROOT: C:/Users/joeun/Desktop/workspace/Game-UnityClient. Git 정본: ROOT/UnityClient/Builds/ClientGrowthHUD/source. 실제 Editor: ROOT/UnityClient.
- codex/client-item-usability, origin/master614c61a(PR71·PR72 병합) 기준. 앞선가방UI ecb5131·선택기록fccc447과이번등급포션을한PR로묶는다. 아직새push/PR없음.
- 몬스터AI·사용자Game·기존패킷/ClientCore/DB스키마·저장형식·공급/드롭/상점/자동지급은변경하지않았다.

## 확정·완료
- 사용자A의등급보류는후속명시지시로대체했다. call_aff6463120cd4d1a8d59f70e24fa87ac 승인:기존HP1500/MP500,중급HP3000/MP1000,상급HP4500/MP1500. 전부공용6초. 신규6..9는가방사용만,1/2키기존품목유지. 골드상점제외.
- 장착확정배지·선택장비효과·슬롯상태연결·공용남은초표시,포션판별/응답/설명콘텐츠기반확장완료. 아이콘기존HP/MP재사용,서버Data→JSON5→기존GUID에9품목반영.
- 신규ID확인:지정DB인벤5002행·거래/이력/배송함및정지런타임WAL11건참조1,2,3,4,33. 신규6..9미참조. 운영/과거백업전역보장아님. 기존33무변경.

## 검증·한계
- 앞선UI기존3검사통과. 신규영향서버사용1·UI가방사용1·콘텐츠공유1각통과. 변경없는장착/퀵표시검사는반복하지않았다.
- MainServer/UnitTests Release빌드성공·Unity컴파일오류없음·9품목import/GUID보존확인. 최초프로젝트직접빌드경로문제는솔루션대상빌드로해결했다.
- 코드리뷰필수0·Astra게시후보검수통과필수0·사이트생성/링크검사통과. 샘플UI9품목미리보기는실제보유/소비가아니다.
- 새Player빌드·실서버교체·실전등급소비/획득·입력검증은미실행. 런처DefensiveGear기존산출물유지. 검증용Editor12784정상종료·Playoff/scene clean·서버미기동.
- 상세정책/결과:history/2026-11-07-item-usability/POTION_POLICY.md·TIER_RESULTS.md. 근거:ROOT/UnityClient/Builds/ClientGrowthHUD/verification/item-usability.

## 다음 행동
- 同묶음Slack1794022476.887309으로구체후보게시승인요청1회후push/PR. POTIONTIER01/r1 게시승인대기: 요청1794026275.520149, 명령 승인 POTIONTIER01. 대상db4e51b까지현재묶음push·PR1개,병합제외. 회신미수신/미소비. 정책승인을게시승인으로사용하지않는다.
- 활성화710(신규263/후속447),토큰·비용N/A. 실제총괄직접작업/검사/도구호출제외.
- 기존장비의실전근거·IEQ2/WAL3구서버복구제한은history/2026-11-06-defensive-equipment/RESULTS.md에보존한다.

- 게시요청전수신기복구완료:실제node29380·listening·최근health·고정대상/스레드대조. Slack앱발신요청확인. 코드후보db4e51b,후속변경은이승인대기기록뿐이다.
