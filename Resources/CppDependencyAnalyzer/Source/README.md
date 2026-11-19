# C/C++ 의존성 분석기

Clang으로 C/C++ 소스의 클래스·프로젝트 의존성과 코드 위치를 수집하고, 로컬 HTML 보고서로 탐색하는 Windows 도구입니다.

- [실행 화면과 기능 안내](../README.md): WinForms 설정, 라이브러리 단위 분석, 지도·선택 목록·순환 검사·의존성 트리·후보 검사
- [빌드와 명령줄 옵션](../BUILD.md): 요구 환경, Clang 런타임 준비, 입력 형식과 선택 검사 옵션
- [서버 분석 보고서](https://joeunil.github.io/MMORPG/reports/server-project-map.html): 이 저장소 서버의 생성 시점 분석 결과

## 소스 구성

| 파일 | 역할 |
| --- | --- |
| `Program.cs`, `AnalyzerForm.cs` | 명령줄·WinForms 입력, 분석 실행과 결과 열기 |
| `ProjectFiles.cs`, `CompilationDatabase.cs` | Visual Studio 프로젝트·컴파일 DB의 소스와 빌드 설정 수집 |
| `Analyzer.cs`, `ClangInstallation.cs`, `ClangNative.cs` | Clang 탐색·호출, 파일별 분석·진단 |
| `AstGraph.cs`, `AnalysisResult.cs` | AST의 타입·관계·코드 근거와 결과 모델 |
| `CodeInspections.cs` | 선택한 리팩터링·성능 후보 수집 |
| `HtmlReport.cs`, `Report.html`, `Report.js`, `RelationshipLayout.js` | 독립 HTML 생성, 화면 조작과 지도 배치 |

특정 `.vcxproj`를 선택하고 **참조 프로젝트 포함**을 끄면 그 프로젝트의 소스만 분석합니다. 기본 의존성 지도는 항상 생성하며, 추가 검사 7종은 기본으로 꺼져 있습니다. 클래스 선택 목록은 생성된 보고서의 브라우저 화면에서 관리합니다.
