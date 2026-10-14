#include "nomad_adapter.h"
#include "problems.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t calls;
static const TestProblem *original;

static double CountedObjective(const double *x, int dim)
{
    ++calls;
    return original->function(x, dim);
}

static NomadRun *Run(const TestProblem *problem, int seed, int budget)
{
    original = problem;
    TestProblem counted = *problem;
    counted.function = CountedObjective;
    double start[MAX_DIM] = {0};
    start[0] = problem->lower + 0.83 * (problem->upper - problem->lower);
    start[1] = problem->lower + 0.19 * (problem->upper - problem->lower);
    char error[1024];
    calls = 0;
    NomadRun *run = NomadOptimize(&counted, start, seed, budget, error, sizeof(error));
    if (!run)
    {
        fprintf(stderr, "%s: %s\n", problem->name, error);
        exit(1);
    }
    size_t count = NomadEvaluationCount(run);
    assert(count == calls && count == NomadReportedCount(run));
    assert(count > 0 && count <= (size_t)budget);
    double best = INFINITY;
    double time = 0;
    for (size_t i = 0; i < count; ++i)
    {
        const NomadEvaluation *entry = NomadEvaluationAt(run, i);
        for (int d = 0; d < problem->dim; ++d)
        {
            assert(entry->point[d] >= problem->lower);
            assert(entry->point[d] <= problem->upper);
        }
        assert(entry->value == problem->function(entry->point, problem->dim));
        best = fmin(best, entry->value);
        assert(entry->bestValue == best);
        assert(problem->function(entry->bestPoint, problem->dim) == best);
        assert(entry->elapsedMs >= time);
        time = entry->elapsedMs;
        assert(NomadAtBudget(run, i + 1) == entry);
        assert(NomadAtTime(run, time)->bestValue <= best);
    }
    assert(NomadAtBudget(run, 100001)->bestValue == best);
    assert(NomadAtTime(run, NomadElapsedMs(run))->bestValue == best);
    assert(NomadAtBudget(run, 0) == NULL);
    assert(NomadAtTime(run, -1) == NULL);
    assert(NomadEvaluationAt(run, count) == NULL);
    assert(NomadEvaluationAt(run, 0)->point[0] == start[0]);
    assert(NomadEvaluationAt(run, 0)->point[1] == start[1]);
    printf("%-12s seed=%3d budget=%6d actual=%6zu best=%.17g elapsed=%.1fms\n",
           problem->name, seed, budget, count, best, NomadElapsedMs(run));
    return run;
}

int main(void)
{
    TestProblem *problems[] = {&SphereProblem, &RastriginProblem,
        &RosenbrockProblem, &AckleyProblem, &HimmelblauProblem, &GriewankProblem};
    NomadRun *first = Run(&SphereProblem, 42, 100000);
    for (size_t p = 0; p < sizeof(problems) / sizeof(problems[0]); ++p)
    {
        for (int seed = 0; seed < 3; ++seed)
        {
            NomadRun *run = Run(problems[p], seed, 100000);
            NomadDestroy(run);
        }
        NomadRun *shortRun = Run(problems[p], 7, 100);
        NomadDestroy(shortRun);
    }
    NomadRun *repeat = Run(&SphereProblem, 42, 100000);
    assert(NomadEvaluationCount(first) == NomadEvaluationCount(repeat));
    for (size_t i = 0; i < NomadEvaluationCount(first); ++i)
    {
        const NomadEvaluation *a = NomadEvaluationAt(first, i);
        const NomadEvaluation *b = NomadEvaluationAt(repeat, i);
        assert(a->value == b->value && a->bestValue == b->bestValue);
        assert(memcmp(a->point, b->point, sizeof(a->point)) == 0);
        assert(memcmp(a->bestPoint, b->bestPoint, sizeof(a->bestPoint)) == 0);
    }
    char error[128];
    double badStart[2] = {INFINITY, 0};
    assert(NomadOptimize(&SphereProblem, badStart, 0, 100, error, sizeof(error)) == NULL);
    assert(error[0]);
    NomadRun *recovered = Run(&SphereProblem, 42, 1);
    assert(NomadEvaluationCount(recovered) == 1);
    NomadDestroy(recovered);
    NomadDestroy(first);
    NomadDestroy(repeat);
    puts("PASS: C API, six objectives, exact counts, histories, checkpoints, timing queries, budgets, repeat independence, and invalid-input recovery.");
    return 0;
}
