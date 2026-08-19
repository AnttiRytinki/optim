#include <stdlib.h>
#include <math.h>
#include "optimizers.h"

#define DE_POPULATION_SIZE 40
#define DE_F 0.8
#define DE_CR 0.9

static void CopyVector(double *dst, const double *src, int dim)
{
    for (int i = 0; i < dim; i++)
        dst[i] = src[i];
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

AlgorithmResult DifferentialEvolution(const TestProblem *problem, int maxEvaluations)
{
    AlgorithmResult result;
    result.bestValue = INFINITY;
    result.evaluations = 0;

    double population[DE_POPULATION_SIZE][MAX_DIM];
    double trial[MAX_DIM];
    double values[DE_POPULATION_SIZE];

    for (int i = 0; i < DE_POPULATION_SIZE; i++)
    {
        for (int d = 0; d < problem->dim; d++)
            population[i][d] = RandomDouble(problem->lower, problem->upper);

        values[i] = problem->function(population[i], problem->dim);
        result.evaluations++;

        if (values[i] < result.bestValue)
        {
            result.bestValue = values[i];
            CopyVector(result.bestX, population[i], problem->dim);
        }
    }

    while (result.evaluations < maxEvaluations)
    {
        for (int i = 0; i < DE_POPULATION_SIZE; i++)
        {
            int a = RandomIndexExcept(DE_POPULATION_SIZE, i, -1, -1);
            int b = RandomIndexExcept(DE_POPULATION_SIZE, i, a, -1);
            int c = RandomIndexExcept(DE_POPULATION_SIZE, i, a, b);

            int forcedDimension = rand() % problem->dim;

            for (int d = 0; d < problem->dim; d++)
            {
                double r = (double)rand() / (double)RAND_MAX;

                if (r < DE_CR || d == forcedDimension)
                    trial[d] = population[a][d] + DE_F * (population[b][d] - population[c][d]);
                else
                    trial[d] = population[i][d];

                if (trial[d] < problem->lower)
                    trial[d] = problem->lower;

                if (trial[d] > problem->upper)
                    trial[d] = problem->upper;
            }

            double trialValue = problem->function(trial, problem->dim);
            result.evaluations++;

            if (trialValue < values[i])
            {
                values[i] = trialValue;
                CopyVector(population[i], trial, problem->dim);

                if (trialValue < result.bestValue)
                {
                    result.bestValue = trialValue;
                    CopyVector(result.bestX, trial, problem->dim);
                }
            }

            if (result.evaluations >= maxEvaluations)
                break;
        }
    }

    return result;
}
