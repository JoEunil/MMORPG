# Back Pressure

## 1. 개요

서버는 병목이 발생했을 때 drop으로 대응하는 것을 기본 정책으로 생각한다. 다만 세션 종료처럼 유실할 수 없는 작업까지 동일하게 버릴 수는 없다.  
이 문서는 서버를 구현하면서 고민한 내용을 바탕으로 내가 생각한 back pressure 정책을 정리하고, 이를 `BackPressure`로 일반화해 구현한 기록이다. 아직 실제 서버에는 적용하지 않았으며 단위 테스트로 동작만 검증하였다.  

## 2. 도입 배경

서버를 구현하면서 WAL과 작업 queue의 back pressure 정책을 각각 고민했었다.

- WAL은 병목이 발생하면 degraded mode로 전환하였다.
- 작업 queue도 degraded mode에서 입력을 막는 방식을 고려했지만, 세션 종료 작업이 유실될 수 있어 그대로 적용하지 못했다.
- 작업 queue는 drop을 기본으로 하고, 세션 종료 경로만 `yield + 최대 재시도 횟수 + 실패 로그`를 사용하는 bounded retry로 처리하였다.

이 방식은 단순 drop보다 유실 가능성을 낮추지만, 중요한 경로마다 별도의 예외 처리가 필요하다. 이후 입력의 등급을 나누고, degraded mode에서는 중요도가 낮은 입력을 버리면서 중요한 입력만 별도의 queue에 쌓는 방식이 게임 서버 운영에 적합하다고 판단하였다.  

## 3. 동작 방식

### Normal

- 모든 입력을 bucket에 넣는다.
- bucket이 가득 차면 degraded mode로 전환한다.

### Degraded

- 서비스를 중단하는 대신 품질을 낮춰 핵심 처리를 유지한다.
- `Droppable` 입력은 버린다.
- `Important` 입력은 defer queue에 넣는다.
- Consumer는 기존 bucket을 먼저 비운 뒤 defer queue를 처리한다.
- 두 queue가 모두 비면 normal 상태로 복구한다.

Important는 먼저 처리한다는 의미가 아니라, 병목 상황에서도 가능한 한 유실하지 않는다는 의미다.

## 4. Bucket

Back pressure 정책이 특정 queue 구현에 종속될 필요는 없으므로 bucket은 template 인자로 받는다.

```cpp
template <typename Bucket, typename T, uint32_t deferQSize>
class BackPressure;
```

Bucket은 다음 계약만 지키면 된다.

```cpp
bool push(T& item);
bool pop(T& out);
```

MPMC 환경에서 사용한다면 전달하는 bucket도 MPMC-safe해야 한다.

## 5. 주의사항

- `Enqueue`는 bucket 또는 defer queue가 입력을 받으면 `true`, Droppable을 버리거나 defer queue까지 가득 차면 `false`를 반환한다.
- defer queue도 고정 크기이므로 호출자는 `false` 반환 시 로그, 재시도 또는 별도의 실패 처리를 선택해야 한다.
- 복구와 defer push가 겹치는 경우를 위해 normal 상태에서도 bucket이 비면 defer queue를 확인한다.
- bucket과 defer queue 각각의 FIFO는 유지되지만 전체 입력에 대한 전역 FIFO는 보장하지 않는다.

## 6. 참고

[UnitTests/BaseLib/BackPressure.cpp](UnitTests/BaseLib/BackPressure.cpp)에서 다음을 검증한다.
- Droppable drop, Important defer, drain 및 normal 복구
- `push/pop` 계약만 구현한 custom bucket 사용

[BackPressure.h](BaseLib/BackPressure.h)
