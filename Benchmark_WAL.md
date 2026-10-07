# WAL Benchmark

## 1. 개요
WAL의 fsync 주기를 결정하기 위해, fsync 주기와 writer 수에 따른 처리량을 측정하였다. 

### Fsync() 처리 구조
이 문서에서 fsync 주기는 전용 thread가 WAL::Fsync()를 호출하는 주기를 의미한다. Fsync()는 dirty record가 있을 때 다음 두 단계로 처리한다.  
현재 구현은 두 작업을 하나의 주기로 묶어 process crash와 OS crash의 durability window를 동일한 정책으로 관리한다.    

```cpp
void Fsync() {
    if (!dirty)
        return;

    {
        std::lock_guard lock(m_mutex);
        fflush(m_fp);                  // CRT buffer → OS page cache
        dirty = false;
    }

    {
        std::lock_guard lock(m_syncMutex);
        FlushFileBuffers(m_handle);    // OS page cache → disk
    }
}
```

fflush()는 writer와 동일한 write mutex를 사용하므로 이 구간에서 writer-fflush contention이 발생한다. 반면 FlushFileBuffers()는 write mutex를 해제한 뒤 실행하므로 일반적인 WAL::Write()를 직접 block하지 않는다. 이때 사용하는 sync mutex는 segment rotation이나 종료 과정에서 file handle이 교체되거나 닫히는 것을 방지한다.  
따라서 fsync 주기를 짧게 설정할수록 fflush()가 write mutex를 획득하는 빈도와 disk flush 빈도가 함께 증가한다. 이 벤치마크는 fsync 주기와 writer 수를 변경하여 writer-fflush contention, writer-writer contention 및 disk flush의 영향을 모두 포함한 WAL 기록 처리량을 비교한다.  

## 2. 벤치마크 시나리오
fsync 주기별로 WAL writer의 처리량을 측정한다.   
writer 수는 1개, 3개를 각각 측정한다. 각 조건에서 writer가 `WAL::Write()`를 최소 1초 이상 반복 호출하도록 하고, WAL 기록 처리량을 측정한다. 이 측정을 조건별로 100회 반복하여 평균, 중앙값, 하위 1% 처리량(p01), 하위 5% 처리량(p05)을 집계한다.  
또한 lock contention을 확인하기 위해, fsync 주기 25ms에서 writer 8개인 시나리오도 측정한다.  

fsync 주기와 writer contention에 측정 범위를 한정하기 위해 segment limit을 `UINT32_MAX`로 설정하여 segment rotation을 제외했다. 또한 15-byte fixed-size test payload를 사용하므로, 결과는 실서비스 `WalInventoryRecord`의 절대 처리량이 아니라 조건별 상대적인 처리량 변화로 해석한다.

- writer 수를 고정하고 fsync 주기를 변경하여 fsync 빈도가 Write 처리량에 미치는 영향을 비교한다.
- fsync 주기를 25ms로 고정하고 writer 수를 1개, 3개, 8개로 변경하여 writer contention이 처리량에 미치는 영향을 비교한다.
- 현재 서버 설정과 동일한 writer 3개 결과를 기준으로, 짧은 유실 구간과 처리량 사이의 절충점을 선택한다.

이 결과는 writer-fflush contention과 writer-writer contention이 합산된 최종 처리량을 보여준다. mutex wait time을 각각 직접 측정하는 벤치마크는 아니므로, 개별 contention 비용을 정량적으로 분리하기보다 조건 간 처리량 변화로 영향을 판단한다.

## 3. 결과 분석

전체 측정 결과: [WAL_Result.txt](Benchmark/WAL_Result.txt)

### Writer 3개

현재 서버의 MQ worker 수와 동일한 writer 3개 조건의 결과는 다음과 같다.

| fsync 주기 | 평균 처리량 | p01 처리량 | 평균 대비 p01 | p05 처리량 | 평균 대비 p05 |
|---:|---:|---:|---:|---:|---:|
| 10ms | 166.4 MiB/s | 147.1 MiB/s | 88.4% | 151.8 MiB/s | 91.2% |
| 20ms | 192.4 MiB/s | 173.4 MiB/s | 90.1% | 177.1 MiB/s | 92.0% |
| 25ms | 189.0 MiB/s | 171.3 MiB/s | 90.6% | 175.3 MiB/s | 92.8% |
| 30ms | 191.7 MiB/s | 172.8 MiB/s | 90.1% | 178.6 MiB/s | 93.2% |
| 50ms | 206.2 MiB/s | 183.8 MiB/s | 89.1% | 187.8 MiB/s | 91.1% |
| 100ms | 212.5 MiB/s | 187.5 MiB/s | 88.2% | 195.9 MiB/s | 92.2% |

fsync 주기가 길어질수록 처리량은 전반적으로 증가했지만, 주기가 길어지면 장애 시 유실 가능 구간도 함께 증가한다. 따라서 최고 처리량 자체보다 짧은 주기에서 평균과 하위 처리량이 크게 뒤처지지 않는 구간을 선택하는 것이 목적이다.  
각 주기의 p01은 평균의 88.2%~90.6%, p05는 평균의 91.1%~93.2% 범위로, 하위 처리량의 상대적인 감소 폭은 주기별로 큰 차이가 없다.   
20ms, 25ms, 30ms의 평균 처리량 차이는 최대 약 1.8%이며, p01 차이는 약 1.2%, p05 차이는 약 1.9%이다. 20ms는 세 조건 중 평균과 p01이 가장 높고, 30ms는 p05가 가장 높지만 차이가 작다.  
**유실 가능 구간과 처리량을 함께 고려하여 20ms를 최종 fsync 주기로 선택했다.** 10ms는 유실 가능 구간을 더 줄일 수 있지만, 20ms보다 평균 처리량이 약 13.5% 낮다.  

### Writer 1개

writer contention이 없는 writer 1개 결과는 다음과 같다.

| fsync 주기 | 평균 처리량 | p01 처리량 | 평균 대비 p01 | p05 처리량 | 평균 대비 p05 |
|---:|---:|---:|---:|---:|---:|
| 10ms | 197.6 MiB/s | 174.2 MiB/s | 88.2% | 179.6 MiB/s | 90.9% |
| 20ms | 214.4 MiB/s | 193.2 MiB/s | 90.1% | 198.3 MiB/s | 92.5% |
| 25ms | 223.7 MiB/s | 203.1 MiB/s | 90.8% | 208.4 MiB/s | 93.2% |
| 30ms | 226.8 MiB/s | 206.8 MiB/s | 91.2% | 209.0 MiB/s | 92.2% |
| 50ms | 240.1 MiB/s | 213.6 MiB/s | 89.0% | 219.5 MiB/s | 91.4% |
| 100ms | 241.4 MiB/s | 220.8 MiB/s | 91.4% | 221.2 MiB/s | 91.6% |

writer 1개에서는 fsync 주기가 길어질수록 평균 처리량이 대체로 증가한다. 다만 50ms에서 이미 100ms 평균 처리량의 약 99.5%에 도달하여, 50ms 이후에는 주기를 늘려도 추가 처리량 개선이 거의 없다. 20ms, 25ms, 30ms 사이에서는 주기가 길어질수록 평균 처리량이 증가하지만, 현재 서버의 writer는 3개이므로 fsync 주기 결정에는 위의 writer 3개 결과를 우선한다.

### Writer contention

동일한 25ms fsync 주기에서 writer 수에 따른 결과는 다음과 같다.

| Writer 수 | 평균 처리량 | Writer 1개 대비 | p01 처리량 | 평균 대비 p01 | p05 처리량 | 평균 대비 p05 |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 223.7 MiB/s | 100.0% | 203.1 MiB/s | 90.8% | 208.4 MiB/s | 93.2% |
| 3 | 189.0 MiB/s | 84.5% | 171.3 MiB/s | 90.6% | 175.3 MiB/s | 92.8% |
| 8 | 91.5 MiB/s | 40.9% | 85.9 MiB/s | 93.9% | 86.8 MiB/s | 94.8% |

writer를 1개에서 3개로 늘리면 평균 처리량이 약 15.5% 감소하고, 3개에서 8개로 늘리면 추가로 약 51.6% 감소한다. writer 8개의 평균 처리량은 writer 1개의 약 40.9%이다. WAL의 `Write()` 경로는 하나의 mutex로 serialize되므로 writer를 추가해도 처리량이 증가하지 않고, thread 간 lock contention과 context switching 비용으로 오히려 감소한다.  

## 4. 결론

- 이 벤치마크는 `fflush()`가 write mutex를 획득하면서 발생하는 writer-fflush contention과, 여러 writer가 동일한 write mutex를 획득하면서 발생하는 writer-writer contention을 함께 반영한다. writer 1개 결과는 writer contention이 없는 baseline이고, writer 3개 결과는 현재 서버 설정에서 두 contention이 반영된 처리량이며, writer 8개 결과는 writer contention이 커졌을 때의 한계를 보여준다.
- 10ms는 fsync 빈도를 높여 유실 가능 구간을 가장 짧게 만들지만 writer 3개 평균 처리량이 20ms보다 약 13.5% 낮다. 반면 20ms, 25ms, 30ms는 평균과 하위 처리량 차이가 2% 이내이며, 50ms와 100ms는 처리량이 더 높지만 유실 가능 구간도 크게 늘어난다.
- 따라서 현재 측정 범위에서는 contention 영향을 반영한 처리량을 충분히 유지하면서 유실 가능 구간을 가장 짧게 가져갈 수 있는 20ms를 최종 fsync 주기로 선택했다.
