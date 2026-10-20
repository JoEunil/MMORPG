# Unity MCP 가능 범위와 한계

**Unity 공식 플러그인 + Unity CLI/Pipeline MCP** 기준이다. CoplayDev MCP와 도구명·설정을 섞지 않는다. 현재 공개된 Pipeline `0.8.0-exp.1`의 전체 명령 분류를 확인했으며, 아래 표는 문서상 지원 범위다. 실제 프로젝트의 패키지 버전과 등록 명령이 최종 기준이다.

## 검증 기준

연결 후 실제 프로젝트 경로·패키지 버전·등록 명령을 확인한다. 문서상 지원과 실행 검증 결과를 구분하고, 미검증 항목은 성공으로 보고하지 않는다.

Unity 공식 연결 요구사항은 Unity 6.0 이상이다. 현재 사용하는 `6000.0.47f1`로 먼저 검증하며 Unity 7 업그레이드를 요구하지 않는다. [연결 요구사항](https://docs.unity.com/en-us/unity-cli/unity-pipeline/unity-pipeline-package)

## 기본 명령으로 지원되는 범위

아래는 [공식 Pipeline 명령 목록](https://docs.unity3d.com/Packages/com.unity.pipeline@0.8/manual/index.html)의 전체 15개 분류다. 특정 설치 버전에 모두 있다고 가정하지 않고 연결 후 목록과 인자를 대조한다.

| 분류 | 가능한 작업 |
| --- | --- |
| 에셋·파일 | 생성·검색·임포트·이동·복사·삭제, 임포트 설정, 폴더/텍스트 파일 |
| Scene | 생성·열기·저장, 활성 Scene·계층 조회, 빌드 Scene 목록 |
| GameObject·Component | 생성·검색·위치/회전/크기·부모·태그/레이어 변경, 컴포넌트 추가/제거·속성 조회/변경 |
| Prefab | 생성·인스턴스·Variant, Override 적용/복원, 내부 편집·Unpack |
| 스크립트 | 생성·연결·직렬화 필드 변경, 프로젝트 코드 실행. 코드 작성은 구현 에이전트가 담당 |
| 애니메이션 | AnimationClip/곡선, Animator 파라미터·레이어·상태/전이, Timeline 구성 |
| Material·Shader | 재질 속성·Shader 연결·키워드 변경, Shader 조회 |
| Bake | 조명·NavMesh·Occlusion 굽기/정리와 비동기 결과 확인 |
| 검색·선택 | Unity Search 질의, Editor 선택 읽기/변경 |
| 캡처 | Game View·Scene View·UI Toolkit 요소의 PNG |
| 빌드·컴파일·테스트 | 빌드 대상/설정, Player 빌드, 재컴파일, 테스트 실행/진행 결과 |
| 프로젝트 설정 | Audio·Graphics·Input·Physics·Player·Quality·Tags/Layers·Time 설정 |
| Package Manager | UPM 패키지 목록·검색·추가/제거·의존성 Resolve·진행 확인 |
| Editor 제어·진단 | Play/Stop/Pause, 메뉴·포커스, Console·성능 통계, 작업 경로 설정 |
| Player Runtime | 연결 설정된 개발 빌드의 상태·종료·프레임률·시간 배율·로그·코드 실행/리로드 |

Unity 설치·프로젝트 생성·라이선스·소스 관리 등은 CLI의 별도 기능도 활용한다. **CLI 기능 전부가 MCP 명령으로 자동 노출되는 것은 아니다.** UPM은 연결된 Editor의 Package Manager 명령과 관련 API로 처리할 수 있다. [CLI 범위](https://docs.unity.com/en-us/unity-cli/unity-cli-reference), [UPM 명령](https://docs.unity3d.com/Packages/com.unity.pipeline@0.8/manual/commands/package-manager.html)

## 우리 프로젝트에서 추가 구현이 필요한 부분

| 목표 | 가능 여부와 필요한 것 |
| --- | --- |
| 쿼터뷰 맵·캐릭터·HUD·인벤토리·거래소 UI | 가능. 위 명령과 UI/게임 코드를 조합한다. UI Toolkit/uGUI 선택과 화면 명세는 별도로 정한다 |
| 지형·VFX·오디오·특수 Shader | 관련 Unity API/패키지와 에셋을 이용해 구성 가능. 모든 편집기 작업에 전용 MCP 명령이 있는 것은 아님 |
| 클릭 이동·키보드 스킬 반복 테스트 | 가능. 실제 입력 경로를 통과하는 테스트 코드와 서버 결과 assertion이 필요. Input System을 쓰면 테스트용 마우스/키보드 입력을 구성할 수 있음 |
| 로그인·두 클라이언트·거래소 E2E | 가능하나 서버/계정/아이템·재화와 실제 프로토콜 연결이 필요. MCP가 게임 서버 API를 만들어주지는 않음 |
| GIF·영상 자동 녹화 | Recorder 등 추가 패키지와 시작/종료 자동화로 가능. 기본 캡처 명령은 PNG이며 Recorder는 Editor Play mode용 |
| FPS·메모리·병목 조사 | 기본 성능 통계와 추가 Profiler/API 계측으로 가능. 정상 플레이·녹화·부하 측정을 구분하고 대상 장비에서 확인 |
| 몬스터·스킬·맵 데이터 export | 작은 Editor exporter로 가능. 서버 파일 적용·DB·프로토콜 변경은 기존 사전 승인 대상 |
| 부족한 Editor 조작 추가 | 등록된 `eval`/C# 실행 또는 작은 커스텀 명령으로 확장 가능. 사용 가능한 API·패키지·Editor 상태의 제약을 받음 |

입력 모사는 [Input System 테스트 기능](https://docs.unity3d.com/Packages/com.unity.inputsystem@1.14/manual/Testing.html)을 활용할 수 있다. 실제 사용하는 패키지 버전에 맞춰 검증한다. 거래소는 현재 내부 캐시/DB 기능만 확인됐으며 클라이언트 접속 경로는 추가 확인 대상이다. 해당 서버 연결이 없으면 S04 승인 전까지 Unity 거래소 E2E는 차단 상태로 둔다.

PNG는 [Game View 캡처](https://docs.unity3d.com/Packages/com.unity.pipeline@0.8/manual/commands/capture.html)의 `screen` 경로가 Play mode의 Overlay UI를 포함한다. 특정 카메라 렌더는 Overlay UI를 포함하지 않는다. [Recorder](https://docs.unity3d.com/Packages/com.unity.recorder@5.1/manual/index.html)는 GIF/영상/이미지 시퀀스를 지원하지만 Standalone 빌드 녹화에는 별도 방법이 필요하다.

## 실질적인 한계

1. **도구 연결이 곧 테스트 완성을 뜻하지 않는다.** 입력 시나리오·합격 조건·테스트 데이터는 먼저 작성한다. 화면 캡처만으로 서버 처리 성공을 판정하지 않는다.
2. **아트 품질은 별도 작업이다.** 에셋 임포트·배치·재질·애니메이션 구성은 가능하지만 완성도 높은 모델·리깅·텍스처 생성은 별도 제작 도구/리소스가 필요하다. 플러그인 설치가 모든 생성 서비스를 제공하지 않는다.
3. **모든 실행 파일을 바로 조작하지는 못한다.** Pipeline의 Player 연결은 해당 패키지/설정을 포함한 개발 빌드가 필요하다. 일반 배포 EXE에 자동 접속하는 기능으로 보지 않는다. [Runtime 조건](https://docs.unity3d.com/Packages/com.unity.pipeline@0.8/manual/runtime-setup.html)
4. **Editor 상태와 버전 영향을 받는다.** 컴파일 오류·Safe Mode·리로드·팝업·긴 Bake·도구 timeout이 흐름을 막을 수 있다. CLI/패키지는 실험 단계이므로 버전을 고정하고 재연결·미완료 작업 확인 절차를 둔다. 고해상도 캡처/렌더링은 실제 그래픽 환경도 검증한다.
5. **동시에 같은 Editor를 편집하지 않는다.** 여러 인스턴스 지정은 가능하나, 우리 워크플로우는 Editor 조작 담당 한 명과 고정된 테스트 대상을 유지한다.
6. **Coordinator·GitHub·Slack·게임 서버 관리는 별도 계층이다.** 역할 배정/리뷰/재시도는 Codex Coordinator, 커밋/게시와 서버 프로세스는 각각 파일·Git·실행 도구가 맡는다. MCP 설치만으로 8개 역할의 상시 무인 실행이 구성되지는 않는다.
7. **MCP가 서버 수정 금지를 강제하지 않는다.** Editor에서 실행하는 C#도 파일 접근이 가능하므로 지침과 실제 실행 권한을 함께 적용한다. 현재 서버 승인 정책은 그대로 유지한다.

## 연결 후 남은 실측 확인

1. CLI 경로/버전, 지정 프로젝트의 Pipeline 버전, `unity status`로 대상 확인.
2. `unity list` 또는 태그를 붙인 명령 조회로 **실제 등록된 모든 도구와 인자** 확인. 공개 목록과 다른 항목만 짧게 기록.
3. 전용 테스트 Scene에서 생성→속성 변경→저장/재열기→컴파일→작은 테스트→HUD 포함 캡처 검증.
4. 그 뒤 실제 서버의 이동/스킬→두 계정 동기화→연결이 준비된 거래소→대표 GIF 순서로 검증.

참고 명령은 `unity command --caller plugin --skill unity-cli --project-path <대상>`이다. 이번에는 실행 가능한 CLI와 대상 프로젝트 연결이 없어 실제 명령 목록 조회까지 수행하지 않았다. 구축 순서는 [MCP_SETUP.md](MCP_SETUP.md), 테스트/캡처 수용 조건은 [DELIVERY.md](DELIVERY.md)를 따른다.
