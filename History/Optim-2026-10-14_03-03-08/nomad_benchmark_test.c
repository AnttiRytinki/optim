/* Tests benchmark instrumentation without changing any optimizer algorithm. */
#define main OptimBenchmarkMain
#include "opt_bench_interactive.c"
#undef main
#include <assert.h>
#include "ScoutsInteractiveD.h"

static double SyntheticObjective(const double *x, int dim)
{
    (void)dim;
    return x[0];
}

int main(void)
{
    TestProblem synthetic = SphereProblem;
    synthetic.function = SyntheticObjective;
    BeginMeasurement(&synthetic, 0);
    double point[MAX_DIM] = {0};
    for (int i = 1; i <= 301; ++i)
    {
        point[0] = i == 101 ? 1.0 : 10.0;
        MeasuredObjective(point, 2);
    }
    assert(measuredCalls == 301);
    assert(measuredValues[0] == 10.0);
    assert(measuredValues[1] == 1.0);
    assert(isnan(measuredValues[2]));

    BeginMeasurement(&synthetic, 1);
    measuredStart = GetTimeMs() - 1000.0;
    point[0] = 0;
    MeasuredObjective(point, 2);
    for (int i = 0; i < TIME_CHECKPOINT_COUNT; ++i)
    {
        assert(isinf(measuredTimes[i]));
    }
    assert(measuredWindowCalls == 0 && isnan(measuredHit));

    unsigned int seed = RunSeed(0);
    srand(seed);
    ScoutsDOptimizer.Init(&SphereProblem);
    double x, y, value;
    int count;
    do
    {
        ScoutsDOptimizer.Step();
        ScoutsDOptimizer.GetBest(&x, &y, &value, &count);
    } while (count < 100000);
    double baselineX = x, baselineY = y, baselineValue = value;
    int baselineCount = count;

    srand(seed);
    TestProblem instrumented = SphereProblem;
    instrumented.function = MeasuredObjective;
    BeginMeasurement(&SphereProblem, 0);
    ScoutsDOptimizer.Init(&instrumented);
    do
    {
        ScoutsDOptimizer.Step();
        ScoutsDOptimizer.GetBest(&x, &y, &value, &count);
    } while (measuredCalls < 100000);
    assert(x == baselineX && y == baselineY && value == baselineValue);
    assert(count == baselineCount && count == measuredCalls);
    assert(measuredValues[CHECKPOINT_COUNT - 1] >= value);
    assert(measuredSuccessEvaluation > 0);
    puts("PASS: exact checkpoint sampling, time deadlines exclude future values, and ScoutsD trajectory unchanged by instrumentation.");
    return 0;
}
