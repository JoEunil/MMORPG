# Triple Buffer Benchmark

## 1. 개요

이 문서는 `TripleBuffer::Write()`의 busy-spin 제한값을 결정하고, `yield` 적용이 SPMC 처리량에 미치는 영향을 확인하기 위한 벤치마크를 정리한다.  
Triple Buffer의 reader는 최신 snapshot을 반영할 때 `0x8000` 비트를 획득한 뒤 `back1`과 `back2`를 교환한다. 이 짧은 구간에 reader가 선점되면 writer는 reader가 다시 실행될 때까지 CAS를 반복한다. 정상적인 짧은 경합은 spin으로 처리하되, 스케줄링 지연으로 길어진 경합에서는 CPU를 계속 소모하지 않도록 일정 횟수 이후 `std::this_thread::yield()`를 호출한다.

## 2. 벤치마크 시나리오

공통 조건은 writer 1개와 reader 1~4개의 SPMC 구성이다. snapshot은 포인터로 교환되므로 payload 크기와 무관하게 atomic flag 및 포인터 교환 경로의 경합을 측정한다. 각 조건은 최소 3초 동안 실행한다.

### Spin 분포

`TripleBufferSpinCounter`는 `yield` 없이 writer가 권한을 얻을 때까지 수행한 CAS 실패 횟수를 반환한다. 경합이 없었던 write는 제외하고, 경합이 발생한 write의 spin 횟수에 대해 p95, p99, 최댓값을 계산한다.  
percentile은 함수 내부의 raw write 표본을 대상으로 한 번만 계산한다. 

### Throughput

다음 두 구현을 같은 reader 수에서 비교한다.  
- Busy-spin: 제한 없이 CAS를 반복하는 `TripleBufferSpinCounter`
- Yield: CAS가 12회 연속 실패하면 `yield`하는 `Base::TripleBuffer`

writer 처리량(`writes/s`)과 전체 reader 처리량(`reads/s`)을 함께 측정한다. 이 테스트는 어느 구현이 항상 빠른지를 증명하기보다, bounded spin이 writer와 reader의 CPU 배분 및 전체 동작에 미치는 영향을 확인하는 데 목적이 있다.

## 3. Spin 결과

전체 측정 결과: [TripleBufferSpin.txt](Benchmark/TripleBufferSpin.txt)

| Reader 수 | 경합 발생 write | spin p95 | spin p99 | spin max |
|---:|---:|---:|---:|---:|
| 1 | 14.30% | 4 | 6 | 248,051 |
| 2 | 25.03% | 5 | 7 | 277,713 |
| 3 | 18.66% | 6 | 8 | 69,542 |
| 4 | 16.05% | 6 | 8 | 96,071 |

모든 조건에서 p99는 8 이하이다. 따라서 정상적인 경합의 99%는 8회 이내의 CAS 실패로 끝났다.  
반면 최댓값은 약 7만~28만 회로 p99와 차이가 매우 크다. p99는 상위 1% 표본을 제외하지만 최댓값은 실행 중 단 한 번 발생한 stall도 포함한다. reader가 `0x8000`을 획득한 뒤 flag를 해제하기 전에 선점되면 writer가 그 시간 동안 계속 CAS를 수행할 수 있으므로 이와 같은 긴 꼬리가 발생할 수 있다. 다만 spin 횟수만으로 컨텍스트 스위칭, 인터럽트, 캐시라인 starvation을 구분할 수는 없다.  
이 최댓값은 spin 한도를 높일 근거가 아니다. 오히려 bounded spin으로 빠져나와야 하는 스케줄링성 이상치로 해석한다.   

## 4. `maxSpin = 12` 선택

측정된 spin p99의 최댓값은 8이다. 여기에 50%의 여유를 둔 12회를 busy-spin 한도로 선택했다.

- 정상적인 짧은 경합은 대부분 yield 없이 완료된다.
- p99 이후에도 CAS를 4회 더 허용하므로 측정 오차와 실행 환경 변화에 대한 여유가 있다.
- 추가 CAS 4회의 비용은 일반적으로 `yield`나 컨텍스트 스위칭 비용보다 작다.
- 12회를 넘는 긴 경합에서는 CPU를 계속 태우는 것보다 다른 스레드에 실행 기회를 주는 편이 목적에 맞다.

현재 구현은 CAS가 실패할 때 횟수를 증가시키며, 12번째 연속 실패 후 `yield`한다. 이후에도 획득하지 못하면 매 실패마다 다시 `yield`한다.  
이 값은 현재 하드웨어와 Windows 스케줄러에서 얻은 경험적 기준이다. 다른 CPU, OS, thread 배치에서는 분포가 달라질 수 있으므로 범용 최적값이나 진행 보장으로 해석하지 않는다.

## 5. Throughput 결과

전체 측정 결과: [TripleBufferThroughput.txt](Benchmark/TripleBufferThroughput.txt)

| Reader 수 | Busy writes/s | Yield writes/s | Write 차이 | Busy reads/s | Yield reads/s | Read 차이 |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 15.56M | 14.34M | -7.8% | 1.97M | 1.89M | -3.9% |
| 2 | 15.30M | 8.49M | -44.5% | 1.36M | 2.44M | +79.8% |
| 3 | 6.77M | 6.85M | +1.2% | 2.70M | 2.20M | -18.6% |
| 4 | 5.82M | 5.98M | +2.8% | 2.65M | 2.51M | -5.1% |

yield가 모든 조건에서 처리량을 높이지는 않는다. reader 1개에서는 짧은 경합을 계속 spin하는 편이 writer와 reader 모두 조금 빠르다. reader 2개에서는 busy-spin이 writer CPU 점유를 유지하여 write 처리량이 높고, yield는 reader에 실행 기회를 넘겨 read 처리량이 높아지는 뚜렷한 trade-off가 나타났다. reader 3~4개에서는 writer 처리량이 비슷하거나 yield 쪽이 근소하게 높다.  
Reader 수가 늘어도 writer의 spin 횟수가 선형으로 증가하지는 않는다. writer는 reader의 전체 참조 구간이 아니라 `back1`/`back2` 교환 구간에만 막히며, reader count가 계속 0보다 크면 swap 발생 빈도가 오히려 줄 수도 있다. 여기에 OS thread 배치와 스케줄링이 더해지므로 특정 reader 수에서 결과가 비선형적으로 달라질 수 있다.    
따라서 이 결과는 yield의 절대적인 throughput 우위를 보여주지 않는다. `maxSpin = 12`의 목적은 평균 처리량을 항상 높이는 것이 아니라, 정상적인 짧은 경합은 spin으로 처리하면서 reader 선점으로 인한 극단적인 busy-spin을 제한하는 것이다.  

## 6. 한계

- 각 조건을 한 번씩 측정했으므로 수 퍼센트 수준의 차이는 실행 간 노이즈일 수 있다.
- thread affinity를 고정하지 않아 Windows scheduler의 core 배치 영향을 포함한다.
- 자연 발생한 경합만 측정했으며 reader 선점을 임계구역 안에서 강제로 주입하지 않았다.
- `spin_max`는 실행시간과 표본 수가 늘어날수록 커질 가능성이 있으므로 임계값 선택에 사용하지 않는다.

## 7. 결론

- 정상 경합의 spin p99는 reader 1~4개에서 최대 8이었다.
- p100에서 수만~수십만 회의 긴 spin이 발생하여 bounded spin의 필요성이 확인됐다.
- p99에 50% 여유를 둔 `maxSpin = 12`를 선택했다.
- throughput 결과에는 busy-spin과 yield의 절대적인 우위가 없었으며, reader 수와 스케줄링에 따라 writer/read 처리량의 trade-off가 달라졌다.
- 따라서 12는 정상 경합의 처리량을 최대한 유지하면서 스케줄링성 장기 stall의 CPU 낭비를 제한하기 위한 절충값이다.
