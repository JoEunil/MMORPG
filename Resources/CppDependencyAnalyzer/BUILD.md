# C/C++ 의존성 분석기 빌드 및 실행

[활용 가이드](README.md) · [분석기 소스](Source/)

Windows에서 폴더를 선택하면 Clang으로 C/C++ 소스를 분석하고 기본 브라우저에서 HTML 보고서를 엽니다. 프로젝트 참조와 클래스 상속·멤버 타입·정적 호출을 탐색하고 각 관계의 파일과 행을 확인할 수 있습니다. 분석과 보고서 열기는 로컬에서 동작합니다.

## 실행

`ServerDependencyAnalyzer.exe`는 Windows x64와 .NET 8 Desktop Runtime을 사용합니다. **폴더 선택**으로 분석할 소스 루트를 선택하고 **분석 설정**에서 입력을 고릅니다. 보고서는 `%LOCALAPPDATA%\ServerDependencyAnalyzer\Reports`에 저장됩니다. **결과 열기**는 이번 실행의 보고서를 열고, **결과 폴더**는 저장된 보고서를 파일 탐색기에서 보여줍니다.

- **Visual Studio**는 `.vcxproj`와 구성·플랫폼을 선택합니다. 이 저장소의 서버만 보려면 `MainServer/MainServer.vcxproj`를 선택합니다.
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
.\bin\Release\net8.0-windows\ServerDependencyAnalyzer.exe
```

빌드 후 같은 `Source` 폴더에서 명령줄 분석도 실행할 수 있습니다. 종료 코드 `2`는 오류가 있거나 분석할 파일이 없는 보고서, `1`은 입력 또는 실행 실패, `130`은 취소입니다.

```powershell
dotnet .\bin\Release\net8.0-windows\ServerDependencyAnalyzer.dll --analyze "C:\Game" --entry MainServer\MainServer.vcxproj --configuration Debug --platform x64 --output .\report.html
dotnet .\bin\Release\net8.0-windows\ServerDependencyAnalyzer.dll --analyze "C:\Project" --entry "C:\Build\compile_commands.json" --output .\report.html
```

## 관계 범위

- **고수준 후보**는 서로 다른 타입 2개 이상이 직접 참조하며 나가는 연결이 없는 타입을 모읍니다. Interface·추상 타입은 기본으로 제외하며, 설계상 고수준인지는 코드의 역할로 판단합니다.
- **의존성 검사 → 순환 의존**은 같은 타입으로 돌아오는 경로를 찾습니다. Interface 필터의 타입으로 들어가는 연결은 기본으로 제외하며 포함 여부를 선택할 수 있습니다.
- **전체 클래스**에서 타입을 우클릭해 **의존성 트리 펼치기**를 선택하면 사용처와 의존 대상을 양방향으로 따라갈 수 있습니다.
- **struct 표시**는 지도에서 구조체를 숨깁니다. 순환 검사와 의존성 트리에는 숨긴 타입도 포함됩니다. [화면별 활용법](README.md#37-고수준-후보-찾기)을 참고하세요.
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
