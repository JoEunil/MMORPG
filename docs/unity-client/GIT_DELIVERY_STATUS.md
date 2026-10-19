# GitHub 전달 상태 및 커밋 후보

## 최신 — 로컬 스냅샷 커밋 완료 / 원격 미반영

Coordinator가 C06 제품 변경 전에 명시 manifest1324개/약28MB로 첫 통합 검증 스냅샷을 로컬 커밋했다: `a2296d9` (`feat: snapshot validated Unity field client and disconnect fixes`). 원본 기준은1d5a3d6. `.codex`/루트AGENTS/실행WAL/빌드/원본캡처/비밀파일/C06계약문서는제외했다. 원격push/PR/병합/공개는없다. 원격master1d5a3d6와작업브랜치없음을root의실제ls-remote로도확인했다.

manifest는로컬Captures/pre-c06-commit-manifest.txt와nul. 새staged후검사에서Unity직렬화YAML/기본템플릿/보존patch공백경고가있어재저장하지않았다. 자체제품소스공백차단없음(추가진단2곳은Unity기본TutorialInfo ReadmeEditor). 토큰패턴검사는staged내용을대상으로파일명만조회해의심없음/제외경로없음. 전체보안스캔을대신하지않는다. 단위테스트는소스동일한이미완료B05/기존검증근거를사용하며스냅샷만을위해기능테스트를반복하지않았다. 아래미커밋상태는감사시점이력이다.

확인일: 2026-10-19 (KST)

## 현재 상태

- 기준 HEAD: `1d5a3d69074a1de6bafde30f1ec4dfe5cac1a951` (`test: add zone tick and worker queue benchmarks`).
- GitHub `JoEunil/MMORPG`의 `master`와 위 커밋이 동일함을 원격 비교로 확인했다.
- 원격 `codex/unity-ai-client` 브랜치는 GitHub 브랜치 검색 결과 없다.
- 현재 작업 트리는 수정 tracked 21개와 미추적 1,314개 파일이 있다(이 manifest 1개 포함). 아직 작업 커밋·push·PR은 없다. 따라서 **개발 작업은 GitHub에 반영되지 않았다**.
- 실제 원격 확인은 GitHub 커넥터로 했다. 로컬 `git ls-remote`는 네트워크 초기화 오류를 반환했다.

## 권장 커밋 경계

**C01–C05와 현재 검증된 R01 통합 스냅샷을 첫 작업 커밋으로 준비하고, 이후 C06을 별도 커밋으로 분리한다.** 기존 변경이 한 작업 브랜치에서 함께 발전했고 ClientCore·서버·Unity 에셋·회귀 검증이 서로 맞물린다. 지금 기능별 과거 시점으로 재구성하면 검증된 조합을 잃거나 다른 작업 변경을 잘못 떼어낼 수 있다. C06 파일은 현재 후보에서 제외하고 독립 검증 후 다음 커밋에 포함한다.

## 커밋 후보 manifest — stage 하지 않음

1. 기존 수정 tracked 파일 21개: `.gitignore`; `CacheLib/Config.h`, `CacheLib/Handler.cpp`; `ClientCore/Initializer.cs`, `Interfaces.cs`, `Network/ReceiveTick.cs`, `Network/TCPSocket.cs`, `PacketHelper/Packet.cs`, `Services/AuthService.cs`, `Services/NetworkService.cs`, `ViewData.cs`, `ViewModel.cs`; `CoreLib/LobbyZone.h`, `MessageQueueHandler.h`, `NonZoneHandler.cpp`, `NonZoneHandler.h`, `NonZoneThreadPool.cpp`, `NonZoneThreadPool.h`, `PacketWriter.h`, `StateManager.h`; `UnitTests/UnitTests.vcxproj`.
2. 작업 파일: `UnitTests/CoreLib/NonZoneThreadPool.cpp`.
3. Unity 소스 후보: `UnityClient/Assets/`의 C01–C05 작업물과 필요한 공용 리소스, Unity 프로젝트의 `Packages/manifest.json`, `Packages/packages-lock.json`, `ProjectSettings/`, `.vsconfig`, `Tools/`, `Assets/` 밖의 실제 소스·설정 파일. 현재 미추적 UnityClient 파일은 1,230개이며 C01 13개, C02 트리 1,017개, 나머지는 공용·테스트·도구·프로젝트 설정에 분포한다. `Assets/C06*` 및 C06 작업 중 새로 생긴 파일은 제외한다.
4. 작업 문서: `docs/unity-client/`의 C01–C05/R01/V04/B05 설계·리뷰·진행·검증 근거. 이후 작성된 C06 자료는 제외한다.

실제 stage 직전에 `git status --short --untracked-files=all`과 파일별 diff를 다시 대조해 이 manifest를 고정한다. `git add -A` 또는 UnityClient 전체 디렉터리 무검토 추가는 사용하지 않는다. Unity 에셋 `.meta` 파일은 해당 에셋과 함께 포함한다.

## 제외 및 주의 항목

- `.codex/`, 루트 `AGENTS.md`: 로컬 작업 설정/지침 파일이며 제품 변경 커밋 후보가 아니다. 별도 요청이 없으면 제외한다.
- `backpressure.1`: 실행 데이터 파일이므로 제외한다.
- `UnityClient/Library/`, `Temp/`, `Logs/`, `UserSettings/`, `Captures/`, `Builds/`, `obj/`, `.vs/`, 생성된 `.sln`·`.csproj`: `.gitignore`에서 제외되는 로컬 캐시·빌드·실행 결과다. Unity `Assets`의 필요한 소스 `.dll`은 예외 규칙을 확인한다.
- 미추적 파일명 검색에서 별도의 환경 비밀 파일은 나오지 않았다. `Login/.env`는 현재 기준 커밋에 이미 추적된 파일이며 이번 변경으로 추가되는 후보는 아니다. 내용을 문서나 로그에 복사하지 않고, 원격 공개 전 저장소 기존 파일의 취급을 검토한다.
- 비밀 키워드 검색은 코드의 환경변수 이름/검증 코드만 찾았으며 실제 값은 출력·기록하지 않았다. 이는 전체 저장소 비밀 스캔 도구의 결과로 오해하면 안 된다.
- 원래 `Game` 작업 폴더는 이 환경의 읽기 권한 거부로 별도 상태를 재확인하지 못했다. 해당 폴더에는 쓰기를 수행하지 않았다.

## 검증 근거와 다음 단계

- 현재 증거는 HANDOFF의 C01–C05/R01/V04/B05 완료 기록 및 `Captures/b05-batch-final.txt`다. 마지막 배치에서 로그인·캐릭터 선택·진입, 외곽 이동, 정상 종료 후 DB 저장 관찰과 재접속 상태 일치를 확인했다. 관찰 시점과 기록된 제한은 해당 실행 요약에 따른다.
- 현재 worktree의 `git diff --check`는 오류 없이 끝났다. 이는 공백 검사이며 전체 테스트나 전체 untracked diff 리뷰를 뜻하지 않는다.
- 다음: C06 변경을 별도 단위로 완료·리뷰·배치 검증한 뒤, Coordinator가 C01–C05/R01/V04/B05 후보 전체를 파일별 검토하고 좁은 경로만 stage한다. 커밋 이후 PR 생성 직전에 변경 효과·검증·위험·복구를 짧게 정리해 최종 승인을 요청한다. 승인 전 push·PR 생성·병합·공개는 하지 않는다.
