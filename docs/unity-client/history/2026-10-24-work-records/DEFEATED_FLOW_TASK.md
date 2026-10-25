# HP0 상태 안내와 정상 퇴장

현재 작은 HP0 문구를 기존 HUD와 어울리는 명확한 안내와 로그인 화면으로 돌아가는 버튼으로 보완한다. 서버의 회복·부활·아이템 사용을 새로 만들거나 재접속 시 HP가 회복된다고 안내하지 않는다.

- 계약: 서버는 퇴장 당시 HP를 저장하고 진입 때 적재한다. 실제 DB 저장 성공은 이번 조사에서 확인하지 않았다. Unity CanAct는 HP>0을 요구한다. 기존 종료는 C01LoginScreen.ConnectionFailed → ReleaseClient → ClientChanged(null)이다.
- 수정: C05CombatScreen과 기존 C05CombatTests의 관련 흐름만 구현 담당이 소유한다. Scene·에셋·서버·ClientCore는 변경하지 않는다. Coordinator만 Editor를 조작한다.
- 완료: HP0에서 안내·정상 종료 버튼이 보이고 행동 차단은 유지된다. 살아 있거나 월드 밖·모델 교체·disable일 때 숨기고 종료 때 이벤트와 객체를 정리한다. 로그인 복귀를 캐릭터 선택이나 부활로 표현하지 않는다.
- 최소 검증: 구현·독립 리뷰 동결 후 기존 관련 시나리오 1개와 전용 계정 실제 로그인 → HP0 안내 → 정상 퇴장 1회. 서버 상태가 달라졌거나 HP0을 확인할 수 없으면 임의 상태 주입 없이 미확인으로 남긴다. 새 전투·전체 회귀는 하지 않는다.
- 게시: 이전 사냥 묶음 e16fcf4/HUNT01은 동결 보존한다. 이 후속은 승인 대기 후보에 섞지 않고 다음 관련 사용자 흐름 묶음에 모은다.

계약 조사: hunting_recovery_contract(server_analyst/gpt-6.1-sol medium) 신규1, 읽기만 수행. 근거는 CoreLib/ZoneState_Skills.cpp, StateManager.cpp, CacheLib/Handler.cpp, CoreLib/MessageQueueHandler.cpp, Unity C02WorldScreen 및 C01LoginScreen이다.
