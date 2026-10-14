#include <stdlib.h>
#include <math.h>
#include "optimizers.h"

#define RANDOM_SEARCH_TYPE 3

static const TestProblem *activeProblem;
static AlgorithmResult result;

static double currentX[MAX_DIM];
static double currentValue;

static double LocalRandomDouble(double min, double max)
{
    return min + (max - min) * ((double)rand() / (double)RAND_MAX);
}

static void CopyVector(double *dst, const double *src, int dim)
{
    for (int i = 0; i < dim; i++)
        dst[i] = src[i];
}

static void RandomSearchInteractiveInit(const TestProblem *problem)
{
    activeProblem = problem;

    result.bestValue = INFINITY;
    result.evaluations = 0;

    for (int i = 0; i < problem->dim; i++)
    {
        result.bestX[i] = 0.0;
        currentX[i] = 0.0;
    }

    currentValue = INFINITY;
}

static void RandomSearchInteractiveStep(void)
{
    if (activeProblem == NULL)
        return;

    for (int i = 0; i < activeProblem->dim; i++)
        currentX[i] = LocalRandomDouble(activeProblem->lower, activeProblem->upper);

    currentValue = activeProblem->function(currentX, activeProblem->dim);
    result.evaluations++;

    if (currentValue < result.bestValue)
    {
        result.bestValue = currentValue;
        CopyVector(result.bestX, currentX, activeProblem->dim);
    }
}

static int RandomSearchInteractiveGetPointCount(void)
{
    return 1;
}

static void RandomSearchInteractiveGetPoint(int index, double *x, double *y, int *type)
{
    (void)index;

    *x = currentX[0];
    *y = currentX[1];
    *type = RANDOM_SEARCH_TYPE;
}

static void RandomSearchInteractiveGetBest(double *x, double *y, double *value, int *evaluations)
{
    *x = result.bestX[0];
    *y = result.bestX[1];
    *value = result.bestValue;
    *evaluations = result.evaluations;
}

InteractiveOptimizer RandomSearchOptimizer =
{
    "RandomSearch",
    RandomSearchInteractiveInit,
    RandomSearchInteractiveStep,
    RandomSearchInteractiveGetPointCount,
    RandomSearchInteractiveGetPoint,
    RandomSearchInteractiveGetBest
};
