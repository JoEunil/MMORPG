# 서버 디자인 패턴

기존 서버에 적용한 디자인 패턴과, 『C++ 소프트웨어 디자인』을 읽은 뒤 리팩토링하면서 적용한 사례를 정리한다.

## 1. 어댑터 패턴 — DB 조회 결과 읽기에 적용

- **기존 문제.** DB 조회 결과를 MySQL 타입으로 반환해 Handler·Storage·WAL 복구 코드까지 드라이버 API에 의존했다.
- **목적.** 조회 결과를 읽는 계약을 서버 쪽에 두고, 드라이버별 변환과 소유권 처리를 분리한다.
- **적용.** 공통 결과 읽기 인터페이스를 정의하고, MySQL 어댑터가 실제 결과를 소유해 읽기 요청을 위임한다.
- **효과.** 기존 읽기 흐름과 parser 테스트를 유지하면서 결과 타입 의존성을 DB 계층으로 모았다. 다른 DB의 결과도 같은 인터페이스에 맞춰 제공할 수 있다.

분리 범위는 조회 결과 읽기다. 연결·쿼리·바인딩과 SQL 예외 처리의 MySQL 의존성은 남아 있다.

<details>
<summary>코드 보기</summary>

```cpp
class IResultReader {
public:
    virtual ~IResultReader() = default;

    virtual bool next() = 0;
    virtual uint64_t getUInt64(std::string_view column) const = 0;
    // 나머지 읽기 메서드 생략
};

class MySQLResultAdapter final : public IResultReader {
    std::unique_ptr<sql::ResultSet> result_;

public:
    explicit MySQLResultAdapter(std::unique_ptr<sql::ResultSet> result);
    ~MySQLResultAdapter() override;

    bool next() override;
    uint64_t getUInt64(std::string_view column) const override;
    // 나머지 읽기 메서드 생략
};

uint64_t MySQLResultAdapter::getUInt64(std::string_view column) const {
    const sql::SQLString label(column.empty() ? "" : column.data(), column.size());
    return result_->getUInt64(label);
}
```

[IResultReader.h](CacheLib/DB/IResultReader.h) · [MySQLResultAdapter.cpp](CacheLib/DB/MySQLResultAdapter.cpp)

</details>

## 2. Modern C++ Visitor — 워커 큐 이벤트 처리에 적용

- **기존 문제.** 하나의 이벤트 구조체에 여러 이벤트의 필드를 함께 두고, enum에 따라 필요한 필드를 해석했다. 이벤트 종류와 데이터가 맞는지 호출부에서 관리해야 했다.
- **목적.** 이벤트별 데이터를 분리하고, 기존 워커와 Handler의 실행 경계를 유지한다.
- **적용.** Chat·Zone·NonSimulation 이벤트를 개별 구조체와 variant로 표현한다. 기존 처리 진입점에서 타입별 Visitor로 처리 메서드에 연결한다. 람다나 이벤트별 Handler 클래스는 추가하지 않는다.
- **효과.** 이벤트 종류와 데이터가 타입으로 연결된다. 새 이벤트를 추가하면 variant 목록과 처리 오버로드를 함께 갱신하며, 처리 누락을 컴파일 단계에서 확인한다. 큐의 우선순위·종료 정책과 상태 소유권은 유지한다.

고전적인 Visitor의 가상 함수 기반 이중 디스패치 대신, `std::variant`와 `std::visit`을 사용하는 Modern C++ Visitor 방식이다. 이벤트 타입 집합은 컴파일 시 정해지고, 실제 저장된 타입에 맞는 처리는 실행 시 선택된다.

### 선택하지 않은 구조

처음에는 공통 이벤트 인터페이스 상속, 이벤트 풀, 이벤트별 Handler와 Facade를 고려했다. 다형적 이벤트를 포인터로 전달할 때의 할당 비용을 풀로 관리하고, 처리 분기도 객체로 분리하려는 구상이었다.

하지만 Zone의 진입·이전·거절 등은 같은 상태 전이 흐름에 연결되어 있어 이벤트마다 독립 Handler로 나누기 애매했다. 기존 Handler가 처리 진입점 역할을 하고 있어 별도 Facade를 추가할 필요도 작았다. 이 방향은 현재 문제에 비해 할당·풀 수명 관리와 호출 경로를 늘린다고 판단했다.

따라서 이벤트 필드가 섞인 문제만 해결하는 variant를 선택했다. variant 자체는 이벤트 저장을 위한 별도 힙 할당 없이 큐 슬롯에 값을 보관한다. 내부 문자열·포인터 대상의 할당은 별개이며, 슬롯 크기는 가장 큰 이벤트와 타입 구분·정렬 공간에 따라 정해진다.

<details>
<summary>코드 보기</summary>

```cpp
using NonSimulationEvent = std::variant<NonSimulationWorkEvent,
    NonSimulationDisconnectEvent, NonSimulationLobbyReturnEvent>;

// NonSimulationHandler 내부
struct EventVisitor {
    NonSimulationHandler& handler;

    void operator()(NonSimulationWorkEvent& event) const;
    void operator()(NonSimulationDisconnectEvent& event) const;
    void operator()(NonSimulationLobbyReturnEvent& event) const;
};

// NonSimulationHandler.cpp
void NonSimulationHandler::EventVisitor::operator()(NonSimulationWorkEvent& event) const {
    handler.Process(std::move(event.work));
    event.work.reset();
}

void NonSimulationHandler::EventVisitor::operator()(NonSimulationDisconnectEvent& event) const {
    handler.Disconnect(event.sessionID);
}

void NonSimulationHandler::EventVisitor::operator()(NonSimulationLobbyReturnEvent& event) const {
    handler.ReturnToLobby(event.transfer);
    event.transfer.reset();
}

void NonSimulationHandler::Process(NonSimulationEvent& event) {
    std::visit(EventVisitor{*this}, event);
}
```

[NonSimulationEvent.h](CoreLib/NonSimulation/NonSimulationEvent.h) · [NonSimulationHandler.cpp](CoreLib/NonSimulation/NonSimulationHandler.cpp)

</details>
