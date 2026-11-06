# 아이템 데이터 통합·기본 무기 장착 — 진행 기록

## 현재확인
PR69원격병합 f205bf16,새branch codex/item-equipment. 서버ItemData5개,현재장착상태없음. CharacterState.attack은기본값이며종료DB저장되므로장착보너스를직접누적하지않는다. InventoryData360byte raw DBblob/WAL sizeof고정. slot/padding숨기기/기본attack덮기안은제외.

## 기술비교와Astra자문
A추천: inventory360prefix보존+명시equipment format tag/equipID8trailer=368,구WALtype1(charID+360)/새type2(charID+368)모두복구. 기존loader는더짧은pre-ring blob도허용해실제길이분포읽기확인필수. 잘못된형식을빈가방으로위장금지. Binventory행별도장착열은스키마와WAL모두변경, Ccharacters열만두면가방판매와권위분리로비추천. 구형직접rollback불가,정상flush/신규WAL해결/변환복구준비필요. sameinventory shard에서장착과PartialUpdate/ChangeItemLocked음수차감모두배타. 신규장착은WAL성공후확정. baseattack유지/유효공격은넓게더해u16포화,player피해경로만적용. 기존phase피해는매타격때현재상태사용. 장착은아이템ID단위라동일철검스택전체판매잠금으로설명. 월드진입자율hydration필수.

## 독립구현
item_catalog_share소유5파일완료: exporter/ServerContent/importer/ItemPresentation/기존ContentImportTests2개수정. items단일배열로name/type/weight/회복/쿨정본공유,아이콘로컬기존보존. itemUse미사용중복제거. 현재서버5행고정개수검증대신유일양수ID/필수포션1·2/의미검증,추가ID100fixture와없는icon확인준비. 기존처치보상상수삭제후exporter참조남음결함도제거.
총괄UI에Content인자연결/기존OS MalgunFont참조재사용/fixturemetadata명시. exporter빌드및JSON4실제생성성공5items,기존MonsterData size_t→u8경고1. Unitycompile0error0warning. EditorImportValidated실행5items,GUID e20a847d2d4fa094088b01f4c513165b 유지(meta파일hash차이는CRLF차이로GUID동일확인). 실제asset만canonical로복사. 자동검사/실입력/새서버빌드는미실행. 서버장착/DB/WAL/네트워크변경없음.

## 선택·다음행동
현채팅비동기질문: 기본철검+2/1칸/보유품만/가방유지/판매전해제/재접속유지/타격시점기준과저장형식확장·구서버복귀제약을한번에제안,답변대기. DB스키마/실제DB수정승인으로가정하지않음. 코드확정후전체묶음리뷰와변경영향만검증.
