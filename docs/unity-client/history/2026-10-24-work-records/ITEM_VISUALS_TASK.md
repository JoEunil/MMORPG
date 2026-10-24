# 가방·거래소 아이템 표현 묶음

목표: 기존 아이템 자산을 크게 보고 수량·선택·가격을 쉽게 읽는 게임 UI로 가방 카드/상세와 거래소 행을 함께 개선한다. 조회·구매·취소·수령 및 실패/불명 결과의 의미는 변경하지 않는다.
포함: C07 화면/Setup/기존fixture, C09 화면/Setup/기존fixture, C02 scene의 영향받는geometry만. 기존resources icon재사용/unknownID fallback/원문가격·수량/선택상태보존. 가짜아이템/빈슬롯capacity표시·장착/소비·등록/서버/DB변경은제외.
소유: bag_item_visuals는C07 3파일, market_item_visuals는C09 3파일. root만문서/Scene·Editor/통합. 두작업독립병렬, Assets동시같은파일저작0.
검증: 구현·독립리뷰뒤 compile및기존C07/C09fixture각1개(직접2개),한실제접속에서I빈가방/B실제거래소조회와행선택만확인. 구매/취소/수령쓰기·반복사냥·이전스킬회귀·서버재빌드0. 캡처최대2장. 아이템있는가방의레이아웃은fixture근거로만기록하고실서버가방에주입하지않는다.
종료: geometry가잘리지않고아이콘·수량·이름·가격이읽힘/선택이정상/실제실패는그대로기록. 검증뒤스킬설명후속과관련UI한큰PR로모음. PR41은open/mergeabletrue·head a7bbc1a확인,신규reviewthread0. 기존PLAY05는이미소비한PR41승인이며새UI게시승인으로재사용하지않는다.
배정: 전문신규2(각client_implementer지정gpt-6-luna/medium),누적신규26/재활성화52/78/tokenN/A. 새실제입력은아직시작안함.