# 빌드 및 실행

[프로젝트 소개로 돌아가기](README.md)

## 1. 개발 환경

| 항목 | 버전 |
|---|---|
| Client Engine | Unity 6.0 |
| IDE | Visual Studio Community 2022 |
| OS | Windows 11 (x64), 25H2 |
| DB | MySQL 8.4.0 |
| Session Store | Redis 3.0.504 (Windows용 마지막 버전) |
| Game Server | C++20, Windows SDK 10.0 |
| Client Core | .NET Standard 2.0 |
| DummyClient | .NET 8.0 |
| Login | Node.js 22.14.0 |

> Redis Windows 빌드는 2016년 이후 업데이트가 없으며, 실서비스에서는 Linux 환경에서 운영하는 것이 일반적이다.

## 2. 외부 모듈

서버 런타임에서 사용하는 외부 라이브러리와 바이너리는 `/External` 경로에 포함되어 있다.
Visual Studio 2022의 C++ 데스크톱 개발 도구와 Windows SDK가 필요하다. 단위 테스트·벤치마크는 각각의 `vcpkg.json`에 선언한 의존성을 복원하고, C# 프로젝트는 NuGet 패키지를 복원해야 한다.

- 포함 모듈: hiredis, spdlog, MySQL Connector/C++, nlohmann/json
- NuGet 패키지: Newtonsoft.Json, libevent2
- x86 또는 Windows 8 이하 환경에서 빌드 시, 해당 타겟에 맞는 라이브러리 바이너리를 `/External` 경로에 재배치해야 한다.
- Linux 및 macOS 환경에서의 빌드는 지원하지 않는다.

## 3. 외부 인프라 설정

**DB**
- 새 DB는 `Resources/DB/init.sql` → `Resources/DB/procedure.sql` 순서로 초기화한다. 테스트 계정과 캐릭터는 5절에서 생성한다.
- 기존 DB의 거래소 취소 저장 프로시저를 갱신하려면 `Resources/DB/procedure.sql`만 다시 적용한다. `init.sql`과 캐릭터 시딩은 재실행하지 않는다. 자세한 내용은 [CANCEL 처리](Bazaar.md#cancel-처리-상세-sp_bazaar_cancel) 참고.
- Host: localhost
- ID/PW: root / 1234

**인증 서버 (Redis)**
- `Program Files\redis-server.exe` 실행
- Host: 127.0.0.1, Port: 6379

**로그인 서버**
- 터미널에서 실행 (또는 Visual Studio에서 실행)
```
cd Login
npm ci
npm start
```
- localhost, 포트 3000

**게임 서버**
1. MainServer 빌드 후 실행
2. 또는 `Resources/Release/MainServer.exe` 실행
   - 실행 파일 직접 사용 시 로그 경로가 변경됨
   - `Resources/monitoring/promtail-config.yaml`에서 path를 `Resources/Release/logs/~~.log`로 변경

## 4. 모니터링 설정

1. Grafana, Loki, Promtail 설치
   - Grafana: https://grafana.com/grafana/download?edition=oss&platform=windows
   - Loki, Promtail: https://github.com/grafana/loki/releases (Assets에서 loki, promtail 다운로드)
2. `Resources/monitoring` 폴더의 `loki-config.yaml`과 `promtail-config.yaml` 수정
   - 파일 저장 경로를 로컬 환경에 맞게 수정
3. `loki.bat`, `promtail.bat` 순서대로 실행
   - bat, yaml 파일을 각각 loki, promtail 실행 파일과 같은 경로에 배치
4. `Resources/grafana.db`를 `C:\Program Files\GrafanaLabs\grafana\data`에 덮어쓰기
5. Grafana 실행
   - ID/PW: admin / admin

## 5. 계정 생성, 캐릭터 생성

1. 로그인 서버와 DB가 실행 중인 상태에서 `node Resources/monitoring/create_users.js`를 실행해 테스트 계정을 생성한다.
2. 계정 생성 완료 후 `Resources/DB/createCharacter.sql`을 한 번 실행해 테스트 캐릭터를 생성한다. 이 SQL은 재실행 시 기존 캐릭터를 재사용하지 않고 INSERT하므로 중복 실행하지 않는다.

## 6. 실행 순서

DB → Redis → 로그인 서버 → 게임 서버

## 7. 클라이언트 테스트

**Dummy Client**
- `DummyClients/Program.cs`에서 접속자 수 설정 후 DummyClients 실행

**Unity Client**
- `Resources/UnityClient/UnityClient.exe`
- 현재 HEAD의 Profile 패킷 변경을 반영하지 않은 과거 데모 실행 파일이다. 최신 서버와의 시연에는 호환 클라이언트 갱신이 필요하다.

**기본 테스트 계정**
- ID: test1 ~ test5000 (`create_users.js` 기준)
- PW: 12345
