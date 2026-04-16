# DB 작업 전용 워커 스레드 분리 리팩토링

## 1. 문제 상황
기존 구조에서는 Handler 워커 스레드가 DB 쿼리를 직접 호출하는 동기 방식으로 동작하였다.  
DB I/O 대기 중 Handler 스레드가 블로킹되어 해당 스레드의 다른 작업 처리가 지연되는 구조적 문제가 존재했다.  

또한 Cache의 DB 호출이 Cache 전용 워커 스레드에서 이루어지는 구조로 인해, __DB I/O 지연이 캐시 처리 지연__ 까지 이어질 수 있는 상황이었다.  

## 2. 수정 사항
__구조 변경__  
DBWorker 클래스를 추가하여 DB 작업을 전용 워커 스레드 풀에서 처리하도록 분리   
Handler는 DBWorker::Enqueue로 작업을 적재 후 즉시 반환  

__LoadFromDB 처리 방식 변경__  
기존: EMPTY 상태 시 동기 DB 조회 후 결과 반환  
변경: EMPTY 상태 시 DB_READING 상태로 마킹 후 즉시 반환, 클라이언트 재시도 방식으로 처리  

__Before__
```cpp
auto conn = connectionPool->Acquire();
auto res = conn->ExecuteSelect(5, key.characterID);
connectionPool->Return(conn);
~~~
// 결과 즉시 반환
Insert(shardIndex, key, result);  // READING → AVAILABLE
return CACHE_STATUS::AVAILABLE;
```

__After__
```cpp
dbWorker->Enqueue([=](DBConnection* conn) {
    // DB 워커 스레드에서 실행
    Result result;
    auto res = conn->ExecuteSelect(5, key.characterID);
    ~~~
    Insert(shardIndex, key, result);  // READING → AVAILABLE
});
return CACHE_STATUS::DB_READING; // READING 상태로 즉시 반환
```
> Enqueue 호출 후 Handler는 즉시 DB_READING을 반환하고,  
실제 DB 조회 및 캐시 상태 전환은 DB 워커 스레드에서 비동기적으로 처리된다.  
클라이언트는 DB_READING 수신 시 재시도하며, DB 워커가 Insert 완료 후  
AVAILABLE 상태로 전환되면 이후 요청에서 정상 응답을 받는다.



## 3. 설계 결정 및 트레이드오프
DB 워커는 IO-bound 작업 특성상 컨텍스트 스위칭이 자연스러운 구조로,  
Lock-Free 큐 대신 mutex + condition_variable 기반 작업 큐를 채택하였다.  
캐시 미스 최초 로딩 시 클라이언트가 재시도해야 하는 구조로 변경되었으나,  
DB 쿼리 완료까지 수~수십ms 수준으로 체감 지연은 없다.  

## 4. 참고
- 관련 커밋: [DB 워커 분리](https://github.com/JoEunil/MMORPG/commit/9caf5ff7788781056c8595e301cf1f4c8f9fd09c#diff-4b7f47e557be8c119db15289ba1cc1a173ab61b7e3e432ea76214cdeb033fcc8R48)