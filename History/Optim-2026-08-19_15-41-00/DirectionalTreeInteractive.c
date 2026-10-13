#include <math.h>
#include <stdlib.h>

#include "optimizers.h"

#define DIRECTION_COUNT 8

#define DIRECTIONAL_TREE_AGENT_TYPE 5
#define DIRECTIONAL_TREE_ACTIVE_TYPE 6
#define DIRECTIONAL_TREE_INACTIVE_TYPE 7

static const TestProblem *activeProblem;
static AlgorithmResult result;

static double currentX[MAX_DIM];
static double currentValue;
static double stepSize;

static int searchDepth;
static int searchActive;

typedef struct
{
    double direction[MAX_DIM];
    double position[MAX_DIM];
    double value;
    int active;
    int evaluated;
} DirectionCandidate;

static DirectionCandidate candidates[DIRECTION_COUNT];

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

static void InitializeDirections(void)
{
    if (activeProblem->dim < 2)
        return;

    for (int i = 0; i < DIRECTION_COUNT; i++)
    {
        double angle =
            2.0 * M_PI * (double)i / (double)DIRECTION_COUNT;

        for (int d = 0; d < activeProblem->dim; d++)
            candidates[i].direction[d] = 0.0;

        candidates[i].direction[0] = cos(angle);
        candidates[i].direction[1] = sin(angle);
    }
}

static void StartSearch(void)
{
    searchDepth = 1;
    searchActive = 1;

    for (int i = 0; i < DIRECTION_COUNT; i++)
    {
        candidates[i].active = 1;
        candidates[i].evaluated = 0;
        candidates[i].value = INFINITY;

        CopyVector(
            candidates[i].position,
            currentX,
            activeProblem->dim);
    }
}

static void DirectionalTreeInteractiveInit(
    const TestProblem *problem)
{
    activeProblem = problem;

    result.evaluations = 0;
    result.bestValue = INFINITY;

    for (int i = 0; i < problem->dim; i++)
        currentX[i] = RandomDouble(problem->lower, problem->upper);

    currentValue =
        problem->function(currentX, problem->dim);

    result.evaluations++;

    CopyVector(
        result.bestX,
        currentX,
        problem->dim);

    result.bestValue = currentValue;

    stepSize =
        (problem->upper - problem->lower) * 0.05;

    InitializeDirections();
    StartSearch();
}

static void EvaluateCandidate(
    DirectionCandidate *candidate,
    int depth)
{
    for (int d = 0; d < activeProblem->dim; d++)
    {
        candidate->position[d] =
            currentX[d]
            + candidate->direction[d]
            * stepSize
            * depth;
    }

    Clamp(candidate->position, activeProblem);

    candidate->value =
        activeProblem->function(
            candidate->position,
            activeProblem->dim);

    candidate->evaluated = 1;

    result.evaluations++;

    if (candidate->value < result.bestValue)
    {
        result.bestValue = candidate->value;

        CopyVector(
            result.bestX,
            candidate->position,
            activeProblem->dim);
    }
}

static int CountActive(void)
{
    int count = 0;

    for (int i = 0; i < DIRECTION_COUNT; i++)
    {
        if (candidates[i].active)
            count++;
    }

    return count;
}

static void RemoveWorstHalf(void)
{
    int activeCount = CountActive();

    if (activeCount <= 1)
        return;

    int removeCount = activeCount / 2;

    for (int r = 0; r < removeCount; r++)
    {
        int worstIndex = -1;
        double worstValue = -INFINITY;

        for (int i = 0; i < DIRECTION_COUNT; i++)
        {
            if (!candidates[i].active)
                continue;

            if (candidates[i].value > worstValue)
            {
                worstValue = candidates[i].value;
                worstIndex = i;
            }
        }

        if (worstIndex >= 0)
            candidates[worstIndex].active = 0;
    }
}

static int GetRemainingCandidate(void)
{
    for (int i = 0; i < DIRECTION_COUNT; i++)
    {
        if (candidates[i].active)
            return i;
    }

    return -1;
}

static void FinishSearch(void)
{
    int winner = GetRemainingCandidate();

    if (winner < 0)
    {
        StartSearch();
        return;
    }

    if (candidates[winner].value < currentValue)
    {
        CopyVector(
            currentX,
            candidates[winner].position,
            activeProblem->dim);

        currentValue =
            candidates[winner].value;
    }
    else
        stepSize *= 0.5;

    double minimumStep =
        (activeProblem->upper - activeProblem->lower)
        * 0.000001;

    if (stepSize < minimumStep)
        stepSize =
            (activeProblem->upper - activeProblem->lower)
            * 0.05;

    StartSearch();
}

static void DirectionalTreeInteractiveStep(void)
{
    if (activeProblem == NULL)
        return;

    if (!searchActive)
        StartSearch();

    int activeCount = CountActive();

    if (activeCount <= 1)
    {
        FinishSearch();
        return;
    }

    for (int i = 0; i < DIRECTION_COUNT; i++)
    {
        if (!candidates[i].active)
            continue;

        EvaluateCandidate(
            &candidates[i],
            searchDepth);
    }

    RemoveWorstHalf();

    searchDepth++;

    if (CountActive() <= 1)
        searchActive = 1;
}

static int DirectionalTreeInteractiveGetPointCount(void)
{
    return DIRECTION_COUNT + 1;
}

static void DirectionalTreeInteractiveGetPoint(
    int index,
    double *x,
    double *y,
    int *type)
{
    if (index == 0)
    {
        *x = currentX[0];
        *y = currentX[1];
        *type = DIRECTIONAL_TREE_AGENT_TYPE;
        return;
    }

    int candidateIndex = index - 1;

    *x = candidates[candidateIndex].position[0];
    *y = candidates[candidateIndex].position[1];

    if (candidates[candidateIndex].active)
        *type = DIRECTIONAL_TREE_ACTIVE_TYPE;
    else
        *type = DIRECTIONAL_TREE_INACTIVE_TYPE;
}

static void DirectionalTreeInteractiveGetBest(
    double *x,
    double *y,
    double *value,
    int *evaluations)
{
    *x = result.bestX[0];
    *y = result.bestX[1];
    *value = result.bestValue;
    *evaluations = result.evaluations;
}

InteractiveOptimizer DirectionalTreeOptimizer =
{
    "DirectionalTree",
    DirectionalTreeInteractiveInit,
    DirectionalTreeInteractiveStep,
    DirectionalTreeInteractiveGetPointCount,
    DirectionalTreeInteractiveGetPoint,
    DirectionalTreeInteractiveGetBest
};
