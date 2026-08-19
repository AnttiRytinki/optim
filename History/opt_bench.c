// opt_bench.c
// Compile:
// gcc -O2 opt_bench.c -lm -o opt_bench

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <string.h>

#include "optimizers.h"

#define RUNS 50

typedef AlgorithmResult (*Optimizer)(const TestProblem *problem, int maxEvaluations);

typedef struct
{
    const char *name;
    Optimizer optimizer;
} Algorithm;

double RandomDouble(double min, double max)
{
    return min + (max - min) * ((double)rand() / (double)RAND_MAX);
}

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

static double Sphere(const double *x, int dim)
{
    double sum = 0.0;

    for (int i = 0; i < dim; i++)
        sum += x[i] * x[i];

    return sum;
}

static double Rastrigin(const double *x, int dim)
{
    double sum = 10.0 * dim;

    for (int i = 0; i < dim; i++)
        sum += x[i] * x[i] - 10.0 * cos(2.0 * M_PI * x[i]);

    return sum;
}

static double Rosenbrock(const double *x, int dim)
{
    double sum = 0.0;

    for (int i = 0; i < dim - 1; i++)
    {
        double a = x[i + 1] - x[i] * x[i];
        double b = 1.0 - x[i];
        sum += 100.0 * a * a + b * b;
    }

    return sum;
}

static void Benchmark(const TestProblem *problem, const Algorithm *algorithm, int maxEvaluations)
{
    double totalValue = 0.0;
    double totalDistance = 0.0;
    double bestValue = INFINITY;
    double worstValue = -INFINITY;
    double totalTime = 0.0;

    for (int run = 0; run < RUNS; run++)
    {
        clock_t start = clock();
        AlgorithmResult result = algorithm->optimizer(problem, maxEvaluations);
        clock_t end = clock();

        double seconds = (double)(end - start) / CLOCKS_PER_SEC;
        double distance = Distance(result.bestX, problem->optimum, problem->dim);

        totalValue += result.bestValue;
        totalDistance += distance;
        totalTime += seconds;

        if (result.bestValue < bestValue)
            bestValue = result.bestValue;

        if (result.bestValue > worstValue)
            worstValue = result.bestValue;
    }

    printf("%-16s %-16s avg value: %12.6f  best: %12.6f  worst: %12.6f  avg distance: %10.6f  avg time: %.6f s\n",
        problem->name,
        algorithm->name,
        totalValue / RUNS,
        bestValue,
        worstValue,
        totalDistance / RUNS,
        totalTime / RUNS);
}

int main(void)
{
    srand((unsigned int)time(NULL));

    TestProblem problems[3];

    memset(problems, 0, sizeof(problems));

    problems[0].name = "Sphere";
    problems[0].dim = 2;
    problems[0].lower = -5.0;
    problems[0].upper = 5.0;
    problems[0].optimum[0] = 0.0;
    problems[0].optimum[1] = 0.0;
    problems[0].optimumValue = 0.0;
    problems[0].function = Sphere;

    problems[1].name = "Rastrigin";
    problems[1].dim = 2;
    problems[1].lower = -5.12;
    problems[1].upper = 5.12;
    problems[1].optimum[0] = 0.0;
    problems[1].optimum[1] = 0.0;
    problems[1].optimumValue = 0.0;
    problems[1].function = Rastrigin;

    problems[2].name = "Rosenbrock";
    problems[2].dim = 2;
    problems[2].lower = -3.0;
    problems[2].upper = 3.0;
    problems[2].optimum[0] = 1.0;
    problems[2].optimum[1] = 1.0;
    problems[2].optimumValue = 0.0;
    problems[2].function = Rosenbrock;

    Algorithm algorithms[] =
    {
        { "RandomSearch", RandomSearch },
        { "HillClimb", HillClimb },
        { "DiffEvolution", DifferentialEvolution },
        { "ExploreExploit", ExploreExploitSwarm }
    };

    int problemCount = sizeof(problems) / sizeof(problems[0]);
    int algorithmCount = sizeof(algorithms) / sizeof(algorithms[0]);
    int maxEvaluations = 100000;

    printf("Runs per algorithm: %d\n", RUNS);
    printf("Max evaluations per run: %d\n\n", maxEvaluations);

    for (int p = 0; p < problemCount; p++)
    {
        for (int a = 0; a < algorithmCount; a++)
            Benchmark(&problems[p], &algorithms[a], maxEvaluations);

        printf("\n");
    }

    return 0;
}
