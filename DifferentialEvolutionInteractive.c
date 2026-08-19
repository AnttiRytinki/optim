#include <math.h>
#include <stdlib.h>

#include "optimizers.h"

#define DE_TYPE 5
#define DE_POPULATION_SIZE 40
#define DE_F 0.8
#define DE_CR 0.9

static const TestProblem *activeProblem;
static AlgorithmResult result;

static double population[DE_POPULATION_SIZE][MAX_DIM];
static double values[DE_POPULATION_SIZE];
static int currentIndex;

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

static int RandomIndexExcept(int populationSize, int a, int b, int c)
{
    int index;

    do
    {
        index = rand() % populationSize;
    }
    while (index == a || index == b || index == c);

    return index;
}

static void DifferentialEvolutionInteractiveInit(const TestProblem *problem)
{
    activeProblem = problem;

    result.bestValue = INFINITY;
    result.evaluations = 0;
    currentIndex = 0;

    for (int i = 0; i < DE_POPULATION_SIZE; i++)
    {
        for (int d = 0; d < problem->dim; d++)
            population[i][d] = LocalRandomDouble(problem->lower, problem->upper);

        values[i] = problem->function(population[i], problem->dim);
        result.evaluations++;

        if (values[i] < result.bestValue)
        {
            result.bestValue = values[i];
            CopyVector(result.bestX, population[i], problem->dim);
        }
    }
}

static void DifferentialEvolutionInteractiveStep(void)
{
    if (activeProblem == NULL)
        return;

    double trial[MAX_DIM];

    int a = RandomIndexExcept(DE_POPULATION_SIZE, currentIndex, -1, -1);
    int b = RandomIndexExcept(DE_POPULATION_SIZE, currentIndex, a, -1);
    int c = RandomIndexExcept(DE_POPULATION_SIZE, currentIndex, a, b);
    int forcedDimension = rand() % activeProblem->dim;

    for (int d = 0; d < activeProblem->dim; d++)
    {
        double r = (double)rand() / (double)RAND_MAX;

        if (r < DE_CR || d == forcedDimension)
            trial[d] = population[a][d] + DE_F * (population[b][d] - population[c][d]);
        else
            trial[d] = population[currentIndex][d];

        Clamp(trial, activeProblem);
    }

    double trialValue = activeProblem->function(trial, activeProblem->dim);
    result.evaluations++;

    if (trialValue < values[currentIndex])
    {
        values[currentIndex] = trialValue;
        CopyVector(population[currentIndex], trial, activeProblem->dim);

        if (trialValue < result.bestValue)
        {
            result.bestValue = trialValue;
            CopyVector(result.bestX, trial, activeProblem->dim);
        }
    }

    currentIndex++;

    if (currentIndex >= DE_POPULATION_SIZE)
        currentIndex = 0;
}

static int DifferentialEvolutionInteractiveGetPointCount(void)
{
    return DE_POPULATION_SIZE;
}

static void DifferentialEvolutionInteractiveGetPoint(int index, double *x, double *y, int *type)
{
    if (index < 0 || index >= DE_POPULATION_SIZE)
    {
        *x = 0.0;
        *y = 0.0;
        *type = DE_TYPE;
        return;
    }

    *x = population[index][0];
    *y = population[index][1];
    *type = DE_TYPE;
}

static void DifferentialEvolutionInteractiveGetBest(double *x, double *y, double *value, int *evaluations)
{
    *x = result.bestX[0];
    *y = result.bestX[1];
    *value = result.bestValue;
    *evaluations = result.evaluations;
}

InteractiveOptimizer DifferentialEvolutionOptimizer =
{
    "DifferentialEvolution",
    DifferentialEvolutionInteractiveInit,
    DifferentialEvolutionInteractiveStep,
    DifferentialEvolutionInteractiveGetPointCount,
    DifferentialEvolutionInteractiveGetPoint,
    DifferentialEvolutionInteractiveGetBest
};
