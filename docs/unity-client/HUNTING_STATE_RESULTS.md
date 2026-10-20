# 사냥 상태 표시 검증 결과

codex/hunting-state, master5bf5318(PR36 병합) 기준 미커밋 작업. 기존 작업을 덮어쓰지 않고 후속 브랜치로 분리했다.

기존 Full의 level/exp와 Delta4/5를 자기 상태 및 입장 전 보류 Full에 반영했다. 다른 캐릭터의 보류 레벨 오류로 자기 세션을 종료하지 않으며, 자기 레벨 uint→ushort 초과는 기존 실패 경계로 차단한다. 기존 HUD 한 줄에 서버 Lv/원시 EXP를 표시한다. 비율·레벨업 기준·보상 추정은 없다.

몬스터는 검증된 Full/Delta HP/maxHP를 머리 위 Simple Image 비율에 연결한다. 죽음·관측 만료·Zone·로그아웃 시 기존 visual과 함께 정리한다. 독립 리뷰에서 저체력 음수폭과 첫 Renderer만 사용해 큰 몬스터 머리에 겹치는 문제를 수정했다. 보강 테스트의 보조 몬스터 정리도 assertion 약화 없이 수정했다.

자동: HuntingState category 정확3/3 통과(hunting-batch.txt). 최초 발견0건은 성공으로 포함하지 않고 AssetDatabase.Refresh와 실제 새 타입/메서드 로드 확인 뒤 실행했다. 전체103/C++/거래소 회귀0.

첫 실제 로그인에서 Lv1/EXP0는 일치했지만 Camera.main이 preview카메라여서 바가 기울어져 실패로 보존(hunting-live.txt). 정상퇴장/Playstop 후 quarterViewCamera를 읽기전용 ViewCamera로 전달하도록 최소 수정했고 독립 리뷰 PASS. 카메라만 바뀌어 자동3개는 반복하지 않았다. 재확인 시작 때 사용자의 Esc 중단이 있어 멈췄다가 명시 재개 뒤 진행했다.

최종 실제 입력 hunting-b: 전용5011 로그인/선택/ENTER→필드 QuarterViewCamera에서40바의 투영 수평 오차0.2px이내→Ring버튼1회→실제 HP Delta의 몬스터42/44/47=500/1500·바0.3333333 일치→실캡처 hunting-state.png 1장→정상Disconnect/modelnull·바0→Playstop. 패킷/상태 주입0. 원시성장 Lv1/EXP0만 관찰했고 실제 처치·EXP증가·레벨업은 주장하지 않는다. DTO관찰 코드의 fieldID/fieldId 오타1건은 서버요청없이 수정했다.

캡처 시 ownHP4400/MP9960, 정상전투 이후 READ ONLY 최종5011 HP3500/MP9960/좌표103.061752/11.042296 그대로·level1EXP0. 5012 HP10000/MP9999·좌표그대로. 상태를 DB로 복구하거나 재시딩하지 않았다. 서버31796/9999유지·Established0/재기동0, DB직접쓰기0. CancelIoEx1168은 사용자 확인된 정상종료 경합의 비차단 진단이다.

근거: Captures/hunting-freeze.txt(7소스 동결 동일), hunting-batch.txt, hunting-live-final.txt, hunting-state.png. 공개 HUD 페이지에 현재기능/최신실캡처·이전2캡처 이력을 함께 유지한다. 후속 기존수치 내보내기와 큰 PR 하나로 전달하며, 승인된 목표 안의 다음 단계를 계속 진행한다.