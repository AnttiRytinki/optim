#include <math.h>
#include "optimizers.h"

AlgorithmResult RandomSearch(const TestProblem *problem, int maxEvaluations)
{
    AlgorithmResult result;
    result.bestValue = INFINITY;
    result.evaluations = 0;

    double x[MAX_DIM];

    for (int eval = 0; eval < maxEvaluations; eval++)
    {
        for (int i = 0; i < problem->dim; i++)
            x[i] = RandomDouble(problem->lower, problem->upper);

        double value = problem->function(x, problem->dim);
        result.evaluations++;

        if (value < result.bestValue)
        {
            result.bestValue = value;

            for (int i = 0; i < problem->dim; i++)
                result.bestX[i] = x[i];
        }
    }

    return result;
}
