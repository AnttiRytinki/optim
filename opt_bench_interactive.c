#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "optimizers.h"
#include "problems.h"

#define RUNS 50

static double Distance(const double *a, const double *b, int dim)
{
    double sum = 0.0;

    for (int i = 0; i < dim; i++)
    {
        double d = a[i] - b[i];
        sum += d * d;
    }

    return sqrt(sum);
}

static void BenchmarkInteractive(
    InteractiveOptimizer *optimizer,
    const TestProblem *problem,
    int maxEvaluations)
{
    double totalValue = 0.0;
    double totalDistance = 0.0;
    double bestValue = INFINITY;
    double worstValue = -INFINITY;
    double totalTime = 0.0;

    for (int run = 0; run < RUNS; run++)
    {
        double bestX;
        double bestY;
        double value;
        int evaluations;

        clock_t start = clock();

        optimizer->Init(problem);

        do
        {
            optimizer->Step();
            optimizer->GetBest(&bestX, &bestY, &value, &evaluations);
        }
        while (evaluations < maxEvaluations);

        clock_t end = clock();

        double found[MAX_DIM];
        found[0] = bestX;
        found[1] = bestY;

        double distance = Distance(found, problem->optimum, problem->dim);
        double seconds = (double)(end - start) / CLOCKS_PER_SEC;

        totalValue += value;
        totalDistance += distance;
        totalTime += seconds;

        if (value < bestValue)
            bestValue = value;

        if (value > worstValue)
            worstValue = value;
    }

    printf(
        "%-16s %-22s avg value: %12.6f  best: %12.6f  worst: %12.6f  avg distance: %10.6f  avg time: %.6f s\n",
        problem->name,
        optimizer->name,
        totalValue / RUNS,
        bestValue,
        worstValue,
        totalDistance / RUNS,
        totalTime / RUNS);
}

int main(void)
{
    srand((unsigned int)time(NULL));

    TestProblem *problems[] =
    {
        &SphereProblem,
        &RastriginProblem,
        &RosenbrockProblem
    };

    int problemCount =
        sizeof(problems) / sizeof(problems[0]);

    int maxEvaluations = 100000;

    printf("Runs per algorithm: %d\n", RUNS);
    printf("Max evaluations per run: %d\n\n", maxEvaluations);

    for (int p = 0; p < problemCount; p++)
    {
        for (int i = 0; i < OptimizerCount; i++)
            BenchmarkInteractive(Optimizers[i], problems[p], maxEvaluations);

        printf("\n");
    }

    return 0;
}
