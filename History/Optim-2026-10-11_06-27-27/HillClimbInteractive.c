#include <math.h>
#include <stdlib.h>

#include "optimizers.h"

#define HILL_CLIMB_TYPE 4

static const TestProblem *activeProblem;
static AlgorithmResult result;

static double currentX[MAX_DIM];
static double currentValue;
static double stepSize;
static int currentDimension;
static int currentDirection;

static double LocalRandomDouble(double min, double max)
{
    return min + (max - min) * ((double)rand() / (double)RAND_MAX);
}

static void CopyVector(double *dst, const double *src, int dim)
{
    for (int i = 0; i < dim; i++)
        dst[i] = src[i];
}

static void Clamp(double *x, const TestProblem *problem)
{
    for (int i = 0; i < problem->dim; i++)
    {
        if (x[i] < problem->lower)
            x[i] = problem->lower;

        if (x[i] > problem->upper)
            x[i] = problem->upper;
    }
}

static void HillClimbInteractiveInit(const TestProblem *problem)
{
    activeProblem = problem;

    result.evaluations = 0;
    result.bestValue = INFINITY;

    for (int i = 0; i < problem->dim; i++)
        currentX[i] = LocalRandomDouble(problem->lower, problem->upper);

    currentValue = problem->function(currentX, problem->dim);
    result.evaluations++;

    CopyVector(result.bestX, currentX, problem->dim);
    result.bestValue = currentValue;

    stepSize = (problem->upper - problem->lower) * 0.25;
    currentDimension = 0;
    currentDirection = 1;
}

static void HillClimbInteractiveStep(void)
{
    if (activeProblem == NULL)
        return;

    double candidate[MAX_DIM] = { 0.0 };

    for (int i = 0; i < activeProblem->dim; i++)
        candidate[i] = currentX[i];

    candidate[currentDimension] += currentDirection * stepSize;
    Clamp(candidate, activeProblem);

    double value = activeProblem->function(candidate, activeProblem->dim);
    result.evaluations++;

    int improved = 0;

    if (value < currentValue)
    {
        currentValue = value;

        for (int i = 0; i < activeProblem->dim; i++)
            currentX[i] = candidate[i];

        if (value < result.bestValue)
        {
            result.bestValue = value;

            for (int i = 0; i < activeProblem->dim; i++)
                result.bestX[i] = candidate[i];
        }

        improved = 1;
    }

    if (currentDirection == 1)
        currentDirection = -1;
    else
    {
        currentDirection = 1;
        currentDimension++;

        if (currentDimension >= activeProblem->dim)
        {
            currentDimension = 0;

            if (!improved)
                stepSize *= 0.5;
        }
    }
}

static int HillClimbInteractiveGetPointCount(void)
{
    return 1;
}

static void HillClimbInteractiveGetPoint(int index, double *x, double *y, int *type)
{
    (void)index;

    *x = currentX[0];
    *y = currentX[1];
    *type = HILL_CLIMB_TYPE;
}

static void HillClimbInteractiveGetBest(double *x, double *y, double *value, int *evaluations)
{
    *x = result.bestX[0];
    *y = result.bestX[1];
    *value = result.bestValue;
    *evaluations = result.evaluations;
}

InteractiveOptimizer HillClimbOptimizer =
{
    "HillClimb",
    HillClimbInteractiveInit,
    HillClimbInteractiveStep,
    HillClimbInteractiveGetPointCount,
    HillClimbInteractiveGetPoint,
    HillClimbInteractiveGetBest
};
