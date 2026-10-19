# P2 선택 캐릭터 바인딩 보존 — 검증 결과

2026-10-19. C09 작업과 함께 수행한 사용자 P2 수정이며, 거래소 기능 완료 기록과 구분한다.

`CoreLib/LobbyZone.h::ImmigrateChar`에서 세션 shard 잠금 안에 이미 선택된 `characterID`가 0이 아니면서 도착한 캐릭터 ID와 다르면 거절한다. 로비 등록 성공 후에도 바인딩 값이 0일 때만 최초 대입한다. 같은 캐릭터의 중복 로비 등록은 기존 중복 검사 정책을 유지한다. C08의 `StateManager::ChangeZone`에 있는 shard→lobby→destination 잠금과 원자적인 최초 진입 경로는 수정하지 않았다.

동결한 C++ 후보를 격리 빌드한 후, 기존 C08 영향 대상과 신규 회귀를 합친 52개를 한 배치로 실행했다. `gtest_list_tests`로 동일 필터의 실제 52개를 먼저 확인했고, 52/52 PASS, 50 ms, exit 0이었다. P2 두 회귀는 다음과 같다.

- `NonZoneThreadPoolFixture.LateEnterCannotRebindCharacterInEmptyLobbyWindow`: PASS, 0 ms. 로비에서 캐릭터를 제거했지만 zone 값은 0인 창을 구성해 다른 ID의 재바인딩 거절과 기존 ID 보존, 같은 ID의 기존 중복 정책을 확인했다.
- `NonZoneThreadPoolFixture.LateEnterResponseRacingInitialAdmissionCannotReplaceBoundCharacter`: PASS, 1 ms. 실제 최초 Zone 진입과 늦은 성공 응답을 경합시켜 기존 캐릭터와 필드 상태 보존을 확인했다. 진입 후 같은 ID와 다른 ID의 늦은 성공 응답도 로비 재생성이나 추가 응답 송신을 만들지 않았다.

근거: [회귀 실행 원문](../../UnityClient/Captures/c09-server-regression.txt), [실제 대상 목록](../../UnityClient/Captures/c09-server-list.txt). 이 두 회귀는 컴포넌트 경합 검증이며 가능한 모든 스레드 실행 순서를 증명하는 것은 아니다.

현재 실행 중인 서버는 기존 C08이다. 이번에는 격리 후보 빌드와 로컬 fixture 배치만 수행했고, 서버 교체·재시작·배포는 하지 않았다. Unity 실행이나 C09 전체 완료를 이 결과로 주장하지 않는다. 실제 DB·거래·seed·TEST_BAZAAR 실행은 없었다.
