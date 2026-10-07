#include <benchmark/benchmark.h>

BENCHMARK_MAIN();

/*
Google Benchmark 사용법

1. 벤치마크 함수 작성 및 등록

    static void BM_Example(benchmark::State& state) {
        for (auto _ : state) {
            // iteration마다 한 번 측정할 코드
        }
    }

    BENCHMARK(BM_Example)
        ->Arg(1)
        ->UseRealTime()
        ->Unit(benchmark::kMillisecond);

2. 입력값 등록

    Arg(value)
        인자 하나를 등록한다.

    Args({value1, value2, ...})
        여러 인자로 구성된 실행 조건 하나를 등록한다.

    ArgName("name")
        인자 하나의 출력 이름을 지정한다.

    ArgNames({"name1", "name2", ...})
        여러 인자의 출력 이름을 등록 순서대로 지정한다.

    Range(min, max)
        min부터 max까지 지수적으로 증가하는 값을 생성한다. 기본 배수는 8이다.

    RangeMultiplier(multiplier)
        Range가 중간 값을 생성할 때 사용할 배수를 지정한다.

    DenseRange(min, max, step)
        min부터 max까지 선형으로 step씩 증가하는 값을 생성한다.

    Apply(CustomArguments)
        함수에서 여러 인자 조합을 직접 등록한다.

    state.range(index)
        등록한 인자를 index 순서대로 읽는다.

3. 시간 측정 옵션

    UseRealTime()
        실제 시간을 측정한다. sleep, 대기, I/O 시간을 포함한다.

    MeasureProcessCPUTime()
        벤치마크가 생성한 다른 스레드를 포함한 프로세스 CPU 시간을 측정한다.

    UseManualTime()
        자동 시간 측정 대신 state.SetIterationTime()에 전달한 시간을 사용한다.

    Unit(timeUnit)
        Time과 CPU 열의 출력 단위를 지정한다.

    MinTime(seconds)
        repetition 하나를 최소 seconds 이상 측정하도록 iteration 수를 조절한다.

    Iterations(count)
        iteration 횟수를 정확히 count로 고정한다. MinTime과 함께 사용할 수 없다.

    Repetitions(count)
        동일한 벤치마크 조건을 count번 반복하여 통계 표본을 만든다.

4. 통계 및 출력 옵션

    ComputeStatistics("name", function)
        repetition 결과에 사용자 통계 함수를 적용한다.

    DisplayAggregatesOnly(true)
        콘솔에는 개별 repetition을 숨기고 집계 결과만 표시한다.

    ReportAggregatesOnly(true)
        콘솔과 파일 모두 개별 repetition을 숨기고 집계 결과만 기록한다.

5. state 메서드

    state.PauseTiming()
        이후 작업을 측정 시간에서 제외한다.

    state.ResumeTiming()
        시간 측정을 다시 시작한다.

    state.SetIterationTime(seconds)
        UseManualTime() 사용 시 현재 iteration의 측정 시간을 직접 지정한다.

    state.SetItemsProcessed(items)
        전체 논리 처리량을 items_per_second로 기록한다.

    state.SetBytesProcessed(bytes)
        전체 바이트 처리량을 bytes_per_second로 기록한다.

    state.counters["name"] = value
        사용자 정의 값을 결과에 추가한다.

    state.SkipWithError("reason")
        현재 벤치마크를 실패 처리하고 원인을 출력한다.
*/
