# 몬스터 리스폰·페이즈 리뷰 수정

2026-10-20 사용자 요청: PR37 신규 리뷰 두 건을 실제 서버 코드와 대조하고 타당한 부분만 수정한다. 거래소 등 다른 개발 변경은 포함하지 않는다.

## 판단과 수정

- 리스폰 상태 누락은 유효하다. UpdateMonster는 죽은 동안에도 tick을 증가시키고 Respawn 다음 tick부터 남은 phase를 사용한다. MonsterState::Respawn에서 phase·skillStep·tick을 0으로 초기화한다. HP·위치·어그로 등 기존 복구 동작은 유지한다. 콘텐츠의 다중 페이즈를 금지하는 대신 서버의 새 생명 초기화를 바로잡았다.
- 정수 임계값 중복도 유효하다. maxHp=1500에서 0.5005f/0.5004f는 모두 750으로 절삭된다. 서버는 phase를 올린 tick에 continue하므로 같은 gate를 가진 중간 phase의 동작을 건너뛸 수 있다. 기존 float 순서 검사에 더해 전환에 사용하는 non-final 정수 임계값을 엄격한 내림차순으로 제한한다. 최종 phase의 임계값은 전환 gate가 아니므로 비교에서 제외한다. 이 검사는 큰 단발 피해로 여러 phase를 통과하는 기존 전투 동작을 변경하지 않는다.

## 검증과 제한

- 독립 정적 리뷰: 차단 결함 없음.
- Release UnitTests 빌드 성공. MonsterStateTest.RespawnResetsCombatProgressAndRestoresSpawnState 1/1 통과. 독립 2-phase 데이터로 사망·진행 상태를 설정한 뒤 실제 Respawn을 호출해 전투 초기화와 기존 복구값을 확인했다. ZoneState 전체 생명주기나 실서버 처치 시연을 수행한 것은 아니다.
- Unity 직접 영향 테스트 2/2 통과(0.15초). PR37 exporter 격리 probe와 현재 미게시 catalog exporter 각각에서 750/750 거부·기존 출력 보존·750/749 허용·최종 phase 중복 허용을 확인했다. 최초 EditMode 선택은 발견 0개로 실행되지 않았고 통과로 세지 않았다. 기존 PlayMode 어셈블리에서 해당 두 항목만 실행했다. 게임 입력·로그인·실제 콘텐츠 asset 저장·실제 export 실행은 하지 않았다.
- 증거: root UnityClient/Captures/monster-phase-review-native.txt, monster-phase-review-unity.txt, monster-phase-review-unity-zero-editmode.txt; UnityClient/Builds/Monster-Phase-Review/build.txt. 임시 probe 소스는 검증 후 제거했다. root의 확장 catalog exporter/test에는 같은 검사를 적용했으며 기존 catalog 데이터와 다른 수정은 보존했다. 해당 root 보완은 이 최소 PR 후보에 포함하지 않는다.
- 실행 서버 교체·재기동·DB·wire·씬 변경 없음. 공개 기능/대표 화면 변화가 없어 사이트/캡처 갱신과 게임 입력 회귀는 생략했다. 배포 전이라 실행 서버에 적용됐다고 주장하지 않는다.

## 게시 기준 주의

GitHub 조회로 PR37 merged, head 332fbab, merge 0f9b96d를 확인했다. 현재 GitHub master는 f1b5e44이며 0f9b96d의 후손이 아니다. 실제 master에는 C14 exporter/runtime 파일이 없다. 따라서 codex/monster-phase-review는 리뷰 대상이 존재하는 정확한 병합 커밋 0f9b96d에서 분기했다. master가 단순히 오래된 원격 캐시라는 초기 추정은 API 대조로 폐기했다.

현재 master로 바로 PR을 만들면 본 리뷰 외의 기존 변경도 다시 포함될 수 있다. 사용자 변경을 복원/덮어쓰지 않고 이 좁은 수정을 로컬 커밋으로 보존한다. 게시 기준과 복원 범위를 확정한 뒤 별도 최종 Slack 승인 전에는 push/PR을 하지 않는다. PR37FIX01 및 거래소 게시 승인을 재사용하지 않는다. 복구는 이 후속 커밋만 되돌리며 콘텐츠/DB 초기화를 하지 않는다.
