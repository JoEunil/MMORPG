# R01 START 도구 독립 리뷰

2026-10-19. 서버 분석 담당의 정적 검토. 제품/도구 직접 수정·실행 없음. 사람 승인 및 실제 서버 기동 없음.

초기 검토본:

- `Start-R01Candidate.ps1` SHA256 `74BCBBA53EBAE1920CBB97C0F844C9F47A2F2A61248B64EEB424215F2235D55A`
- `R01_START_REQUEST.md` SHA256 `16418081461CA6B32474219F680D27429658421AF2ABC028809CC2D43A494584`

## 초기 판정 — 실제 기동 전 보완 필요

1. **기동 직전 WAL·고정 파일·환경 재검증 부족.** 현재 WAL0/복사는 DB READ ONLY 두 번보다 앞에 있고 이후에는 서버/포트만 검사한다. DB 관찰 중 새 숫자 WAL이나 비empty 기록이 생기면 사전 gate를 지난 채 시작할 수 있다. 보존 당시 WAL 목록/hash와 현재 동일·모두0인지, 이미지/DLL/inspector hash·TEST unset·서버/포트 부재를 spawn 직전에 다시 검사해야 한다. Preflight 자체는 spawn하지 않는다.
2. **정상 준비 문구의 CP949 해석 누락.** 기존 전환 도구는 raw stdout을 UTF8/CP949 양쪽으로 검사하지만 새 도구는 기본 UTF8 StreamReader만 쓴다. 후보의 실제 stdout이 CP949이면 정상 기동을 실패로 판정하고 서버가 남을 수 있다. 기존 좁은 이중 decoding 경로를 유지한다.
3. **프로세스 handle 고정 전에 로그 I/O 실행.** Start-Process 반환 후 Record가 실패하면 반환된 PID의 handle을 고정하기 전에 중단한다. 반환 즉시 handle과 startticks를 고정하고 이후 정확 PID/path/startticks를 비교한다. 모호한 spawn결과를 명시해 후속 운영자가 임의 재시도하지 않게 한다.

추가 증거 보완: 로그4종도 원본/사본 SHA256 일치를 확인하고 hash manifest를 남긴다. 요청의30초는 spawn부터의 deadline으로 명확히 관리하며, 초기 CIM·hash 작업 시간을 timeout 밖으로 빼지 않는다.

이 초기 차단은 실제 기동 분기의 준비 완성도에 대한 것이다. 기존 R04승인을 R05로 확대하지 않았고, 실패 시 q/kill/자동C0복구를 하지 않는 경계, 고정 inspector의 두 행 READ ONLY·stable 비교, 비밀 값 미출력 방향은 분석안과 일치한다. Coordinator가 보완한 최종 고정본을 재검토한 뒤 판정을 갱신한다.

## 보완본 재검토

도구 SHA256 `67F283FC1C3F65476ED0955125AC892E973466853A056D372DDC1B7B63CB39BB`를 직접 다시 읽었다. 위 초기 차단3개는 해소됐다. 로그4종 사본hash 대조/기록, DB조회 후 candidate/inspector·DLL/TEST/emptyWAL 목록/hash 재검사, 반환 즉시 handle→startticks 고정, UTF8/CP949 raw decoding, stderr/error/game 저장실패 검사와 실패 시 자동조작 없음이 확인됐다.

남은 좁은 지적: 요청은 **시작 후30초**인데 Stopwatch가 spawn 뒤 초기 identity/CIM/hash 검사 후 Test-Ready 내부에서 시작한다. spawn 직전 stopwatch를 만들어 readiness 전체에 재사용하고 성공 반환 직전에도30초 초과 여부를 확인해야 한다. 운영 API호출 자체의 강제 timeout/프로세스 종료를 뜻하지 않으며, 늦게 도착한 결과를 성공으로 승인하지 않는 판정 deadline이다. 이 보완 전에는 정확한30초 문구를 그대로 충족했다고 표시할 수 없다. Preflight는 기동하지 않으므로 이 지적과 독립이다.

## 최종 판정 — 지정 운영안 내 코드상 차단 없음

최종 도구 SHA256 **`846FAF75C273C4E8FB2E4FC57DC0AF2D81AF08BC7B92D55DEA990D97147DD0D0`** 직접 확인. spawn 직전 `startupTimer` 생성, Test-Ready의 동일 timer 재사용 및 성공 직전 elapsed≥30 거부를 확인하여 남은 deadline 지적도 해소됐다. 이 timer는 blocking Windows API 자체를 취소하지 않으며 늦은 결과를 readiness 성공으로 판정하지 않는 상한이다.

Coordinator가 전달한 실제 준비 receipt `UnityClient/Captures/r01-start-20261019-124142-072-25560.txt`를 직접 읽었다. 67F283FC… 버전으로 로그4종 hash/사본·빈WAL2 사본·고정파일/무프로세스/빈포트/TESTunset·READ ONLY 두 snapshot stable을 통과하고 PreflightOnly로 끝난 기록이다. exit0은 Coordinator 인계값이며, receipt에도 process start/DB쓰기 없음이 명시돼 있다. 최종846FAF75…의 변경은 startup-only timer 경로이고 Preflight 분기는 동일하므로 해당 준비 근거를 재사용할 수 있다. **최종본 전체 preflight를 다시 실행했다고 주장하지 않는다.** 실제 기동/실SQL 저장/HP 보존 성공 근거는 아니다.

실행 조건은 별도 R01-START-05/r1 사람 승인 확인, 플랫폼 허용, 실행 직전 동일한 환경·보존·고정 파일 guard 통과다. 요청서의 기존 '파서/preflight 미실행' 문장은 Coordinator가 위 실제 준비 결과와 최종hash로 갱신해야 하며 이 문서 리뷰는 운영 허가를 대신하지 않는다. 프로세스 시작 결과 불명/준비 실패 뒤 q·kill·자동C0복구·재시도를 하지 않는 원래 범위는 유지된다.

후속 요청서 SHA256 **`644AD3718E86A5AE34EE91D3DE04F6DB5F34D18C82C8B15066FBF151137918CA`**를 다시 읽었다. 최종846F hash/파서0, 이전67F 실제preflight와 최종 startup-only delta, 실제 기동0 구분이 반영돼 위 문서 지적도 해소됐다. 승인 요청 범위에 추가 차단 없음.
