#include "nomad_adapter.h"
#include "nomad_view.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static NomadRun *history;
static size_t visible;
static int seed;
static char error[1024];

void NomadViewCleanup(void)
{
    NomadDestroy(history);
    history = NULL;
    visible = 0;
}

static void Init(const TestProblem *problem)
{
    NomadViewCleanup();
    if (!problem || problem->dim != 2)
    {
        snprintf(error, sizeof(error), "Visualization requires a two-dimensional problem");
        return;
    }
    double start[MAX_DIM] = {0};
    for (int d = 0; d < problem->dim; ++d)
    {
        start[d] = RandomDouble(problem->lower, problem->upper);
    }
    seed = rand();
    history = NomadOptimize(problem, start, seed, 100000, error, sizeof(error));
    visible = history ? 1 : 0;
    if (!history)
    {
        fprintf(stderr, "NOMAD visualization: %s\n", error);
    }
}

static void Step(void)
{
    if (visible < NomadEvaluationCount(history))
    {
        ++visible;
    }
}

static int GetPointCount(void)
{
    return (int)visible;
}

static void GetPoint(int index, double *x, double *y, int *type)
{
    if (index < 0 || (size_t)index >= visible)
    {
        return;
    }
    const NomadEvaluation *entry = NomadEvaluationAt(history, (size_t)index);
    *x = entry->point[0];
    *y = entry->point[1];
    *type = (size_t)index + 1 == visible ? 5 : 8;
}

static void GetBest(double *x, double *y, double *value, int *evaluations)
{
    const NomadEvaluation *entry = NomadAtBudget(history, visible);
    *x = entry ? entry->bestPoint[0] : 0;
    *y = entry ? entry->bestPoint[1] : 0;
    *value = entry ? entry->bestValue : INFINITY;
    *evaluations = (int)visible;
}

const char *NomadViewStatus(void)
{
    static char status[1200];
    if (!history)
    {
        snprintf(status, sizeof(status), "NOMAD error: %s", error);
    }
    else
    {
        snprintf(status, sizeof(status), "evaluation playback %zu/%zu | seed %d%s",
                 visible, NomadEvaluationCount(history), seed,
                 visible == NomadEvaluationCount(history) ? " | stopped" : "");
    }
    return status;
}

InteractiveOptimizer NomadOptimizer =
{
    "NOMAD", Init, Step, GetPointCount, GetPoint, GetBest
};
