# 전용 빈 inventory 2행 보완 후보 독립 리뷰

2026-10-19. 범위는 `NEXT_ITEM_CONTRACT.md`, 고정 빈 BLOB ABI 후보, `Prepare-C12EmptyInventory.cjs`와 기존 inventory Load/flush/WAL/메시지 경로다. HUD·일반 서버 리팩터링은 제외한다. 리뷰어는 제품 소스·DB·서비스·Editor·Git·Slack을 변경하거나 실행하지 않았다. 이 문서만 작성했다. DB 조회/쓰기·컴파일·codec·Unity 시험 실행은 모두 **0회**다. 다른 담당자의 기존 증거를 읽은 것은 독립 실행 통과와 구분한다.

## 현재 판정

**코드상 차단 문제 없음 — P1/P2 수정과 all-zero codec 추가 재검토 완료.** SELECT-only 후보에는 무승인 INSERT 경로가 없다. 아래 발견 이력과 수정 확인을 구분한다. Coordinator의 codec 실행 PASS 증거를 읽었으며, 지정 사람의 Slack 승인과 정상 종료·drain·WAL 실상태 확인은 아직 충족된 것으로 기록하지 않는다. 이 문서는 DB 쓰기 승인 또는 DB 반영 완료 근거가 아니다.

### P1 — 접속 수 0으로 pending cache/WAL 쓰기를 배제할 수 없음

- 위치: `UnityClient/Tools/Prepare-C12EmptyInventory.cjs:16-18,59-61`의 live listener/Established 검사. 서버 근거: `CacheLib/WALManager.h:62-89`, `CacheStorageInventory.cpp:9-29`, `CacheFlush.cpp:35-117`.
- 발생 조건: DB 행이 없는 캐릭터라도 부팅 시 WAL 복구로 AVAILABLE+dirty 캐시를 가질 수 있다. `RestoreInventory`는 DB 행이 없을 때 dbLSN=0으로 비교하고 `RestoreEntry`로 복구한다. 기존 flush는 UPDATE-only이며 대상 행이 없어 실패하면 dirty를 유지한다. 활성 연결이 0이어도 DBWorker에 이미 받은 요청이나 dirty flush는 남을 수 있다. 세 번의 TCP 스냅샷 사이의 재접속도 배제되지 않는다.
- 영향: 빈 INSERT 커밋 뒤 기존 flush가 다른 inventory 이미지를 기록하거나 pending ring의 Bazaar CLAIM까지 수행할 수 있다. SQL 두 INSERT 자체가 정확해도 승인하려는 빈 2행·아이템0개 범위를 넘는 결과가 생길 수 있다. 새 READ ONLY 검사는 이를 탐지할 수 있지만 되돌리지는 못한다. 현재 실제 서버에 이런 pending이 있다는 확인은 없으며, 위험 경로가 실행 전 조건에서 배제되지 않았다는 지적이다.
- 최소 수정: commit은 고정 테스트 서버의 정상 종료·프로세스 부재·9999 listener/Established 부재와 정상 drain 근거, 같은 작업 디렉터리의 numeric WAL 확인 뒤에만 실행한다. 기본 SELECT-only preflight는 live 상태에서 유지해도 된다. 정상 종료나 WAL 근거가 불명확하면 중단하고 강제 종료·WAL 삭제·가드 완화를 하지 않는다.
- **해결 확인:** 최종 driver의 `executionState()`는 default에서 기존 live 검사, commit에서 `serverStopped()`로 분리했다. 후자는 전체 MainServer 프로세스0, 로컬 또는 원격 9999의 Listen/Established0, root의 redo/backpressure 양 numeric prefix 존재와 일치하는 **모든 segment 0바이트**를 연결전·INSERT전·COMMIT전에 검사한다. 정상 종료·drain 증거는 기존 가드 절차로 확보하는 별도 전제로 명시됐다. 같은 Coordinator가 프로세스 조작권을 유지하는 로컬 시험 범위에서 충분하며 새 서버 동기화 구조는 요구하지 않는다. 비협조적인 별도 서비스나 운영 실행을 안전하게 만드는 범용 도구로 해석하지 않는다.

### P2 — inventory 컬럼의 PK 비참여 검사가 빠짐

- 위치: 같은 driver의 metadata assertion(최초 검토본 41행).
- char_id가 `COLUMN_KEY=PRI`인 것만으로 단독 PK를 확인하지 못한다. 두 컬럼인 테이블에서 inventory 컬럼도 PK에 참여하는 경우를 현재 조건은 배제하지 않는다.
- 실제 컬럼 진단에는 inventory.COLUMN_KEY가 빈 값이므로 확인된 현재 DB에는 이 차이가 없다. **해결 확인:** 최종 assertion에 `columns[1].COLUMN_KEY === ""`가 추가됐다. 두 컬럼 고정이므로 char_id 단독 PK 전제를 확인하며 별도 범용 schema 검사기는 필요 없다.

## 유지할 보호와 계약

- 기본 모드는 `START TRANSACTION READ ONLY` 후 누락/identity 확인과 ROLLBACK으로 반환하므로 INSERT에 도달하지 않는다. `--commit`만 명시 쓰기 모드이고 잘못된 인자는 중단한다. Slack 승인 확인은 Coordinator의 별도 책임이며 CLI 플래그 자체를 사람 승인으로 해석하지 않는다.
- 두 고정 char_id와 user/profile/login/name/channel/미삭제를 모두 비교한다. 기존 inventory가 한 행이라도 있으면 중단하며 `REPLACE`, UPSERT, `INSERT IGNORE`가 없다. commit에서 identity와 누락 PK 범위를 잠그고 INSERT 한 문장·affectedRows=2·길이360/all-zero·정확 두 ID를 확인한다.
- 메타데이터 signed BIGINT는 실제 진단 `Captures/c12-db-columns.txt`와 일치하도록 정확히 수정됐다. 저장소 DDL의 unsigned와 차이가 있으나 두 고정 양수 ID는 범위 안이고 스키마 변경은 없다. 임의 타입 수용으로 완화하지 않았다. InnoDB/컬럼2/EXTRA/트리거0 검사도 유지한다.
- 커밋 시도 전 실패는 ROLLBACK 완료 뒤에만 rollback 메시지를 쓴다. ROLLBACK 자체가 실패하면 상위의 중단/SELECT 확인 메시지로 종료한다. `commitAttempted=true`를 COMMIT 호출 직전에 설정하여 응답 유실을 rollback 성공으로 보고하지 않으며 자동 재시도하지 않는다. commit 뒤 별도 READ ONLY 연결로 확인한다. 이후 실패에도 자동 DELETE가 없다.
- all-zero360은 기존 MsgInventoryItem native8/InventoryData native360의 빈 의미와 맞으며 wire81바이트와 구분된다. 실제 Load는 EMPTY_INVENTORY로 선초기화하고 native 구조체로 읽는다. flush도 같은 native 크기를 복사한다. 정상 빈 행은 후속 Refresh에서 status1/count0을 지원하며 row-missing/status0 정책을 바꾸지 않는다.
- 새 추상화·범용 codec·서버 구조 변경은 없고 고정 후보 2행에 필요한 경계 검사 위주다. 고정 ID/PID 같은 제한은 의도된 실행 범위 보호다. 대상 서버가 바뀌면 확인 없이 값만 바꾸지 않는다.

## 읽은 실행 증거와 남은 검증

root `UnityClient/Captures/c12-layout-compile.cmd`는 MSVC vcvars64, `/std:c++20 /DNDEBUG /Zs`로 실제 헤더의 크기/offset static_assert를 확인하는 명령이다. `c12-layout-compile.txt`는 진단 오류 없이 source basename만 남긴 결과이고 성공 exit0은 Coordinator가 보고했다. 리뷰어가 컴파일을 실행한 것은 아니다.

root `c12-empty-inventory-preflight-final.txt`에는 exact identity/두 행 누락/InnoDB/schema/trigger 일치의 SELECT-only PASS가 있다. `c12-db-columns.txt`에는 비밀 없는 두 컬럼 metadata만 있다. 중간 unsigned 기대값의 사전검사 실패는 실제 signed와 달라 안전 중단한 이력이다. SQL write/commit/오류 rollback 실증과 최종 Unity 실입력은 아직 확인하지 않았다.

### 추가 codec 검토와 Coordinator 실행 증거

기존 ABI 후보 cpp에 `main()`만 추가된 것을 읽었다. 360바이트 zero-initialized 배열을 `memcpy`로 InventoryData 전체에 복사한 뒤 count/head/tail/lastOp/lastLsn, 모든 item의 ID/quantity/slot, 32개 recentEventIds를 검사한다. 실패는 각 nonzero exit code로 전달하고 성공은0이다. padding까지 의미 필드로 해석하거나 일반 아이템 codec 지원을 주장하지 않는다. 고정 all-zero native 이미지의 **decode 검증**이며 별도 serialize round-trip 검증은 아니다. 이 후보에 추가 차단 문제는 없다.

root `UnityClient/Captures/c12-codec-batch.cmd`는 vcvars64와 `/std:c++20 /DNDEBUG`로 실제 cpp를 컴파일·링크하고, 컴파일/실행 오류가 없을 때만 PASS를 출력한다. `c12-codec-batch.txt`의 `PASS ABI and all-zero empty inventory codec`를 직접 읽었고 Coordinator가 실제 실행 exit0을 보고했다. **이는 Coordinator 실행 근거이며 리뷰어의 컴파일/codec 실행은 계속0회다.** DB 데이터·서버 프로세스를 건드리는 코드가 이 cpp에는 없다.

Coordinator는 BAG01 승인 요청을 지정 Slack 부모 `1792407691.861929`에 발신 `1792408332.366609`로 올렸으며 지정 사람의 회신과 DB 쓰기는 아직 없다고 보고했다. 리뷰어가 Slack을 직접 조회하거나 승인 회신을 확인한 것은 아니다. 정상 종료 가드와 DB 승인 경계는 유지한다.

## 기존 종료·WAL 가드 재사용 근거

root `UnityClient/Tools/Switch-R01LocalServer.ps1`의 Get-WalSegments/Assert-EmptyWal, Preserve-LogsAndWal, Save-PersistenceSnapshot, Assert-StopEvidence/Stop-Normally와 `Start-R01Candidate.ps1`의 Assert-ServerAbsent/Assert-PreservedWal이 기존 경계다. C10 서버 image는 `UnityClient/Builds/C10-Delivery/bin/MainServer.exe`이고 WorkingDirectory는 **저장소 root**다. 활성 WAL은 root `redo.<숫자>`/`backpressure.<숫자>`이며 bin 폴더가 아니다. `BaseLib/WAL.h`는 prefix+숫자 segment를 연다. logs는 root `logs/{system,game,error,perf}.log`다. Save-PersistenceSnapshot은 전용 두 캐릭터의 READ ONLY DB 수치 비교이며 inventory BLOB이나 별도 snapshot 복구 파일이 아니다. 가드 실패 시 비우기·삭제·임의 해석으로 성공 처리하지 않는다.

## 읽은 파일 SHA256

최종 재검토 파일의 hash를 아래에 기록한다. root 실행 증거와 후보 소스는 위치를 구분한다. 문서가 기록한 버전 이후 실행 경로 변경이 있으면 영향받는 부분을 재검토한다.
```text
733EABB3FAAF75B73488973F0E32E9852AF06B167044298C373914F1430A7169  UnityClient/Builds/C10-Delivery/source/docs/unity-client/NEXT_ITEM_CONTRACT.md
BF25B690561EA610E681F078D22B8B4A0D9535C7CE2C466E9E649B0F9FE382D9  UnityClient/Builds/C10-Delivery/source/docs/unity-client/empty-inventory-layout.candidate.cpp
A9ED07E7A0B92F6813EB2BDA270DB96340492F217376748071ED236D03049F90  UnityClient/Builds/C10-Delivery/source/UnityClient/Tools/Prepare-C12EmptyInventory.cjs
1D1C3C9FC860871CB63C068DF2C923C51A06DF75109D9D38642F5C333F0BA8E9  UnityClient/Builds/C10-Delivery/source/CacheLib/CacheStorageInventory.h
4CB104F51255F70C111523EE2CAFCB0A5AEEA473A942681112B440DB92ABC9A9  UnityClient/Builds/C10-Delivery/source/CacheLib/CacheStorageInventory.cpp
6280972E32A0B16A9C72AC9A30C01CE553B88707AE5591499EB403FDDDDF2097  UnityClient/Builds/C10-Delivery/source/CacheLib/CacheStorage.inl
9313B864DFCA0429414C546463F69E58D54966998AC71F4FA720115029E96336  UnityClient/Builds/C10-Delivery/source/CacheLib/Handler_Inventory.cpp
BC11859EBEA94F0E3FDC07EE6ACD2EB2C92E46F007FF21D7C146D91B1B30A372  UnityClient/Builds/C10-Delivery/source/CacheLib/Handler_Bazaar.cpp
639A5B97F513F621F80369E2066B4917BE2D5A2C5CB28C894348588A259A5C99  UnityClient/Builds/C10-Delivery/source/CacheLib/WALManager.h
CDA336696354570BB5994EB21D063B665F6B9CD587ED4F132E631DCF394E7FFC  UnityClient/Builds/C10-Delivery/source/CacheLib/CacheFlush.cpp
CEB8D62B83AFEA05680FDC9F711C7911EB951FC0D56A8087467CC81BDCF47C23  UnityClient/Builds/C10-Delivery/source/CacheLib/DBWorker.h
EC2A04E719C50940AE4776562B19F55FD5FFC171D1ACB76DC3650A648AE84981  UnityClient/Builds/C10-Delivery/source/CacheLib/Initializer.h
70C82AF83FE348C4BDE8822B9C099C516A2AA2CD4554503158ADBE32C03F9C0F  UnityClient/Builds/C10-Delivery/source/CacheLib/Config.h
77E4D55E1306165640AFBCE64B7216B05A160F4C67944C767F5E380EE92F7AAB  UnityClient/Builds/C10-Delivery/source/CoreLib/MessageTypes.h
069F26CACDEB57E38013534A071FF7B2F68CEFA26970BCEDBD008620023F8E20  UnityClient/Builds/C10-Delivery/source/Resources/DB/init.sql
52DDDB6F55D978C4169093A8B21D23DA2F18F4247329126E62EDF8CA4325B244  UnityClient/Captures/c12-layout-compile.cmd
25EA219ECF45EFF24EF11BB93001548A53CEE60CA4BAD00C8F1581E6156F79F1  UnityClient/Captures/c12-layout-compile.txt
95019B4EA7BD214429D23E1B08687D1750CDC3B8AA9EB7C2A61717A1C9820C3B  UnityClient/Captures/c12-codec-batch.txt
D77E080952927CBD18C81AB85AED572549A29269D0018A4DEFA2AF6071BA59EB  UnityClient/Captures/c12-codec-batch.cmd
7505D71BBC9437CA885D245D67790D5822CACCBA66C0E151CC6A3CAAC5D9352F  UnityClient/Captures/c12-db-columns.txt
22FF30FBA6345E8840DC118538F4A6967BCBF683D813D553DC757300EE7336DD  UnityClient/Captures/c12-empty-inventory-preflight-final.txt
E2B6E29FA37CCF82E2552A3E8062D8F251F17CAD0CB4ED5EB08B608F924AA449  UnityClient/Captures/c12-inventory-preflight.txt
C26B0C7F2ED18161EBC35E96ABFCCD6F6717FBB25E8F26B166AD7AA35FB2D3AC  UnityClient/Tools/Switch-R01LocalServer.ps1
3CF3AB256FE53683FD083A2340A745C6301682342F34EFC505425BAA17F09FBC  UnityClient/Tools/Start-R01Candidate.ps1
170D9921CAE996F267F02BBCA4F441D8369CFBD339BD46A649A8B801A05D9C0D  UnityClient/Tools/Inspect-R01Persistence.cjs
712F0A55F16A0442E78A483238D072B3F76F4078C72F86EEBE05F8122030D45E  UnityClient/Builds/C10-Delivery/source/BaseLib/WAL.h
6D083ECE39178173E38E9716B26064A9AA1AA736124C2E81627DEECF93D7A25D  docs/unity-client/HANDOFF.md
9B4ADDF219C9B42A906A592A044D3D57A7336D9ADC456A9175D062395B570701  docs/unity-client/roles/reviewer.md
FF13A2449437F59CA1C752C1DDB9042C9127CFEBC157724E1140C04D20C5F4A9  docs/unity-client/PIPELINE_GUIDE.md
```
