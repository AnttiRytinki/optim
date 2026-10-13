#include <math.h>
#include "optimizers.h"

AlgorithmResult HillClimb(const TestProblem *problem, int maxEvaluations)
{
    AlgorithmResult result;
    result.evaluations = 0;

    for (int i = 0; i < problem->dim; i++)
        result.bestX[i] = RandomDouble(problem->lower, problem->upper);

    result.bestValue = problem->function(result.bestX, problem->dim);
    result.evaluations++;

    double step = (problem->upper - problem->lower) * 0.25;

    while (result.evaluations < maxEvaluations && step > 1e-9)
    {
        int improved = 0;

        for (int i = 0; i < problem->dim; i++)
        {
            for (int direction = -1; direction <= 1; direction += 2)
            {
                double candidate[MAX_DIM];

                for (int j = 0; j < problem->dim; j++)
                    candidate[j] = result.bestX[j];

                candidate[i] += direction * step;

                if (candidate[i] < problem->lower)
                    candidate[i] = problem->lower;

                if (candidate[i] > problem->upper)
                    candidate[i] = problem->upper;

                double value = problem->function(candidate, problem->dim);
                result.evaluations++;

                if (value < result.bestValue)
                {
                    result.bestValue = value;

                    for (int j = 0; j < problem->dim; j++)
                        result.bestX[j] = candidate[j];

                    improved = 1;
                }

                if (result.evaluations >= maxEvaluations)
                    break;
            }

            if (result.evaluations >= maxEvaluations)
                break;
        }

        if (!improved)
            step *= 0.5;
    }

    return result;
}
