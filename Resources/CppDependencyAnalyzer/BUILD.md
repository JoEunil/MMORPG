# C/C++ 의존성 분석기 빌드 및 실행

[활용 가이드](README.md) · [분석기 소스](Source/)

Windows에서 폴더를 선택하면 Clang으로 C/C++ 소스를 분석하고 기본 브라우저에서 HTML 보고서를 엽니다. 프로젝트 참조와 클래스 상속·멤버 타입·정적 호출을 탐색하고 각 관계의 파일과 행을 확인할 수 있습니다. 분석과 보고서 열기는 로컬에서 동작합니다.

## 실행

`ServerDependencyAnalyzer.exe`는 Windows x64와 .NET 8 Desktop Runtime을 사용합니다. Release 빌드하면 실행파일과 필요한 DLL·설정·Clang 런타임을 이 폴더 최상위로 복사합니다. 최상위 실행파일로 실행하고 **폴더 선택**으로 소스 루트를, **분석 설정**에서 입력을 고릅니다.

기본 분석은 실행파일 옆 `report.html`을 최신 결과로 갱신하고, `Reports`에 날짜별 사본을 보관합니다. **결과 열기**는 최신 결과를 열고 **결과 폴더**는 날짜별 보고서를 보여줍니다. 이미 열린 페이지는 새로고침해야 새 결과가 보입니다. `--output`을 지정하면 해당 파일만 저장하며, 동봉·게시용 `Reports/server-project-map.html`과 Pages는 자동 갱신하지 않습니다.

Release 빌드는 최상위 `report.html`의 분석 데이터를 보존하면서 원본 화면 템플릿을 갱신합니다. 예전 HTML의 화면만 갱신하려면 `ServerDependencyAnalyzer.exe --refresh-report <HTML>`을 사용합니다.

## Visual Studio 연결

소스 폴더 선택 후 **VS 연결 등록**을 누릅니다. 현재 사용자의 `HKCU\Software\Classes\cppdeps`에 분석기 실행파일을 연결하므로 관리자 설치는 필요하지 않습니다. 실행파일을 옮겼다면 다시 등록합니다. 등록한 소스 폴더는 실행파일 옆 `SourceRoots.json`에 보관하며, 이후 분석한 폴더도 여기에 추가합니다.

보고서의 **VS에서 열기**는 설치된 VS를 `vswhere`로 찾고, 해당 파일의 상위 폴더에서 찾은 단일 솔루션 또는 프로젝트와 함께 파일을 엽니다. 파일 경로와 행 번호는 별도 인자로 전달합니다. 외부 웹 보고서에는 로컬 실행 링크를 표시하지 않습니다.

```powershell
.\ServerDependencyAnalyzer.exe --register-vs "C:\Game"
.\ServerDependencyAnalyzer.exe --refresh-report .\Reports\server-project-map.html
```

- **Visual Studio**는 `.vcxproj`와 구성·플랫폼을 선택합니다. 서버 전체는 `MainServer/MainServer.vcxproj`와 **참조 프로젝트 포함**을 사용합니다. 특정 라이브러리만 분석하려면 해당 `.vcxproj`를 선택하고 **참조 프로젝트 포함**을 끕니다. 이 옵션은 개별 프로젝트 선택 시에만 활성화되며 기본값은 켜짐입니다.
- **CMake 및 다른 빌드 도구**는 `compile_commands.json`을 사용합니다. 소스 루트와 일반 빌드 폴더에서 파일을 찾습니다. 별도 빌드 경로라면 **빌드 DB 선택**으로 지정합니다.
- **소스 폴더**는 빌드 설정이 없을 때 C17/C++20 기본 설정을 적용합니다. 소스 파일이 없으면 헤더를 각각 분석합니다.

CMake의 Ninja 또는 Makefile 생성기에서 `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON`으로 컴파일 데이터베이스를 만들 수 있습니다. 데이터베이스의 Windows 경로와 필요한 SDK·헤더는 분석 PC에서 접근할 수 있어야 합니다.

```powershell
cmake -S . -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

## 빌드

.NET 8 SDK, Visual Studio C++ 도구와 Windows SDK가 필요합니다. 아래 명령은 저장소 루트에서 실행합니다. `Source` 폴더로 이동한 뒤 버전이 고정된 Clang 라이브러리와 내장 헤더를 `obj/clang`에 한 번 준비하고 실행 파일과 함께 복사합니다. 이미 LLVM을 설치했다면 해당 설치를 자동 탐색하거나 `LIBCLANG_PATH`와 `CLANG_RESOURCE_DIR`로 지정할 수 있습니다.

```powershell
Set-Location .\Resources\CppDependencyAnalyzer\Source
.\Get-ClangRuntime.ps1
dotnet build -c Release
..\ServerDependencyAnalyzer.exe
```

빌드 후 같은 `Source` 폴더에서 명령줄 분석도 실행할 수 있습니다. 종료 코드 `2`는 오류가 있거나 분석할 파일이 없는 보고서, `1`은 입력 또는 실행 실패, `130`은 취소입니다.

```powershell
dotnet .\bin\Release\net8.0-windows\ServerDependencyAnalyzer.dll --analyze "C:\Game" --entry MainServer\MainServer.vcxproj --configuration Debug --platform x64 --output .\report.html
dotnet .\bin\Release\net8.0-windows\ServerDependencyAnalyzer.dll --analyze "C:\Game" --entry NetLibrary\NetLibrary.vcxproj --configuration Release --platform x64 --include-project-references false --output .\netlibrary.html
dotnet .\bin\Release\net8.0-windows\ServerDependencyAnalyzer.dll --analyze "C:\Project" --entry "C:\Build\compile_commands.json" --output .\report.html
```

## 선택 검사

`--include-project-references false`는 선택한 Visual Studio 프로젝트의 `ClCompile` 소스만 분석합니다. 기본값 `true`는 참조 프로젝트까지 방문합니다. 전체 프로젝트 선택과 컴파일 DB 입력에는 적용되지 않습니다. 포함 헤더의 타입은 여전히 수집되므로 라이브러리 타입만 남기는 화면 필터와는 다릅니다.

WinForms의 **리팩터링 후보**와 **성능 개선 후보**에서 필요한 검사만 켜고 분석합니다. 추가 검사는 기본으로 꺼져 있으며, 기본 의존성 지도는 항상 생성합니다.

| 분류 | 검사 | 명령줄 옵션 |
| --- | --- | --- |
| 리팩터링 후보 | 사용 멤버 | `--member-usage true` |
| 리팩터링 후보 | 반복 파라미터 | `--parameter-groups true` |
| 리팩터링 후보 | 내부 기능 묶음 | `--class-cohesion true` |
| 리팩터링 후보 | 책임 집중 후보 | `--responsibility-concentration true` |
| 성능 개선 후보 | 반복 객체 복사 | `--loop-copy true` |
| 성능 개선 후보 | 반복문 내 선형 검색 | `--loop-search true` |
| 성능 개선 후보 | 반복 동적 할당 | `--loop-allocation true` |

위 옵션을 기존 `--analyze` 명령 뒤에 붙입니다. 보고서는 **미분석**, **후보 없음**, **후보 발견**을 구분하고 검출 근거와 코드 위치를 표시합니다. 후보는 설계 결함이나 실제 성능 병목을 확정한 결과가 아닙니다.

**책임 집중 후보**는 멤버 타입과 정적으로 해석한 호출 대상이 많은 클래스부터 보여줍니다. 같은 단일 직접 기반 클래스를 공유하는 파생 타입은 한 묶음으로 셉니다. 기반 클래스를 하나로 정할 수 없는 경우는 실제 타입 그대로 셉니다. 후보를 펼치면 묶음별 실제 타입과 멤버 선언·메서드 호출 위치가 나옵니다. 이 검사 결과를 보려면 분석기에서 선택해 보고서를 다시 생성하세요.

로컬 HTML에서는 분석 폴더 안의 코드 파일에 **VS에서 열기** 링크를 제공합니다. 위의 연결 등록 후 사용하며, 브라우저가 외부 앱 실행 확인을 표시할 수 있습니다. VS 로드 상태에 따라 지정 행 이동 결과는 달라질 수 있습니다. 웹 보고서는 코드 위치만 표시합니다.

## 관계 범위

Visual Studio 필터 경로에 `Interface`가 들어 있으면 인터페이스 필터로 처리합니다. 대소문자를 구분하지 않으므로 `InternalInterface`와 그 하위 필터에도 같은 기준을 적용합니다.

- **고수준 후보**는 서로 다른 타입 2개 이상이 직접 참조하며 나가는 연결이 없는 타입을 모읍니다. Interface·추상 타입은 기본으로 제외하며, 설계상 고수준인지는 코드의 역할로 판단합니다.
- **의존성 검사 → 순환 의존**은 같은 타입으로 돌아오는 경로를 찾습니다. Interface 필터의 타입으로 들어가는 연결은 기본으로 제외하며 포함 여부를 선택할 수 있습니다.
- **전체 클래스**에서 타입을 우클릭해 **의존성 트리 펼치기**를 선택하면 사용처와 의존 대상을 양방향으로 따라갈 수 있습니다.
- **struct 표시**는 지도에서 구조체를 숨깁니다. 순환 검사와 의존성 트리에는 숨긴 타입도 포함됩니다. [화면별 사용 안내](README.md#2-보고서-보기)를 참고하세요.
- 프로젝트 참조 지도는 `.vcxproj`의 `ProjectReference`에서 생성합니다. Visual Studio 분석은 프로젝트별 `ClCompile` 항목, 구성·플랫폼 조건, include·define·강제 include를 반영하고, 외부 `.props` 및 복합 조건은 진단에 표시합니다.
- 컴파일 데이터베이스는 파일별 작업 디렉터리, 헤더 경로, 매크로, 타깃과 언어 표준을 사용해 클래스 지도를 생성합니다. 지원하지 않는 플래그 및 응답 파일은 진단에 표시합니다. 분석은 Clang을 로컬 라이브러리로 호출합니다.
- **전체 클래스 맵**에는 분석에서 수집한 클래스와 관계를 모두 표시합니다. **관계 지도**에서 노드의 색은 namespace, 크기는 연결된 클래스 수를 나타냅니다. 연결이 없는 클래스는 따로 모읍니다.
- 체크박스로 namespace를 숨길 수 있습니다. 검색 목록에서 클래스를 선택하면 지도에서 해당 위치를 찾고 연결을 강조합니다. 지도에서 노드를 더블클릭하면 직접 연결을 방향별로 펼칩니다.
- **필터 지도**는 Visual Studio의 .vcxproj.filters에 등록된 선언 파일을 기준으로 클래스를 묶습니다. 그룹과 색은 프로젝트별 필터 경로를 따릅니다. 선의 숫자는 연결된 클래스 쌍 수이며, 선을 펼치면 클래스 관계와 코드 위치를 볼 수 있습니다. **Interface 연결만**을 켜면 Interface 필터의 타입을 향하는 관계만 표시합니다.
- **전체 클래스 → 카드 보기 → 프로젝트 묶음**에서는 프로젝트마다 색을 달리합니다. 다른 프로젝트의 Interface 필터에 속한 타입을 상속하면 선을 강조합니다. **프로젝트 간 Interface 상속만**으로 관련 클래스와 상속을 모아 볼 수 있습니다.
- **namespace별 클래스 맵**에서는 선택한 namespace의 내부 관계를 표시합니다. 외부 연결을 켜면 그 namespace에 직접 연결된 다른 클래스도 표시합니다. 중첩 namespace, 전역 클래스, 파일별 익명 namespace도 선택할 수 있습니다.
- 클래스를 선택하고 **선택 연결만**을 켜면 직접 연결을 방향별로 펼칩니다. 근거 패널에서 들어오거나 나가는 연결을 고르고, 연결선을 눌러 코드 위치를 보거나 다른 클래스로 이동할 수 있습니다.
- **관계 지도**는 연결이 많은 노드를 중심에, 말단 노드를 바깥에 배치합니다. **카드 보기**는 항목 이름을 직사각형 카드로 보여주고, 전체 클래스 탭에서는 namespace 또는 프로젝트별로 묶을 수 있습니다.
- 노드를 끌어 옮기면 연결선도 따라옵니다. 빈 공간을 끌어 지도를 이동하고 휠로 확대합니다. **탐색 패널**과 **근거 패널**은 각각 접을 수 있으며, **배치 초기화**로 현재 지도의 노드를 제자리로 돌립니다.
- 루트 Visual Studio 솔루션에 등록된 C++ 프로젝트를 우선 발견하며, 검색 시 중첩 Git 체크아웃과 벤치마크를 제외합니다. 테스트는 **테스트 포함**을 선택해 추가할 수 있습니다.
- 분석 폴더 안의 외부 코드 폴더(External, vendor, third_party 등)에 속한 클래스는 그래프에서 제외합니다. **외부 라이브러리 표시**가 켜지면 해당 폴더에서 Clang이 확인한 include를 근거로 프로젝트 지도에 라이브러리당 주황색 노드 하나를 표시합니다. 시스템 SDK와 모든 링크 라이브러리의 목록은 아닙니다. 이 표시를 끄려면 명령줄에서 --include-vendor false를 사용합니다.
- 클래스는 Clang이 해석한 프로젝트 소스의 선언을 기준으로 연결합니다. 호출 대상은 정적 타입 기준이며 런타임의 가상 호출 대상은 추적하지 않습니다.
- 오류가 있는 파일에서 Clang이 복구한 관계도 볼 수 있습니다. 진단 탭에서 해당 파일과 Clang 오류를 확인할 수 있습니다. 취소는 분석 중인 파일이 Clang에서 반환되는 시점에 반영됩니다.
