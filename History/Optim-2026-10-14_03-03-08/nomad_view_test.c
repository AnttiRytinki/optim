#include "nomad_view.h"
#include "nomad_adapter.h"
#include "problems.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int calls;
static double CountedSphere(const double *x, int dim)
{
    ++calls;
    return Sphere(x, dim);
}

int main(void)
{
    double start[MAX_DIM] = {0};
    srand(42);
    start[0] = RandomDouble(SphereProblem.lower, SphereProblem.upper);
    start[1] = RandomDouble(SphereProblem.lower, SphereProblem.upper);
    int seed = rand();
    char error[1024];
    NomadRun *reference = NomadOptimize(&SphereProblem, start, seed, 100000,
                                       error, sizeof(error));
    assert(reference);
    TestProblem counted = SphereProblem;
    counted.function = CountedSphere;
    srand(42);
    calls = 0;
    NomadOptimizer.Init(&counted);
    assert(calls == (int)NomadEvaluationCount(reference));
    int actual = calls;
    for (size_t n = 1; n <= NomadEvaluationCount(reference); ++n)
    {
        double x, y, value;
        int evaluations, type;
        const NomadEvaluation *entry = NomadAtBudget(reference, n);
        NomadOptimizer.GetBest(&x, &y, &value, &evaluations);
        assert(evaluations == (int)n && NomadOptimizer.GetPointCount() == (int)n);
        assert(x == entry->bestPoint[0] && y == entry->bestPoint[1]);
        assert(value == entry->bestValue);
        NomadOptimizer.GetPoint((int)n - 1, &x, &y, &type);
        assert(x == entry->point[0] && y == entry->point[1] && type == 5);
        if (n > 1)
        {
            NomadOptimizer.GetPoint(0, &x, &y, &type);
            assert(type == 8);
        }
        NomadOptimizer.Step();
        assert(calls == actual);
    }
    assert(strstr(NomadViewStatus(), "stopped"));
    for (int i = 0; i < 100; ++i)
    {
        NomadOptimizer.Step();
    }
    assert(NomadOptimizer.GetPointCount() == actual && calls == actual);
    NomadOptimizer.Init(&RastriginProblem);
    srand(42);
    calls = 0;
    NomadOptimizer.Init(&counted);
    assert(calls == actual && NomadOptimizer.GetPointCount() == 1);
    NomadViewCleanup();
    assert(NomadOptimizer.GetPointCount() == 0);
    NomadDestroy(reference);
    puts("PASS: replay matches every actual evaluation, preserves best history, makes no extra calls, stops, restarts independently, and cleans up.");
    return 0;
}
