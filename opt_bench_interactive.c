#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "optimizers.h"
#include "problems.h"

#define RUNS 50

static const int Checkpoints[] = {
    100,
    300,
    1000,
    3000,
    10000,
    30000,
    100000
};

#define CHECKPOINT_COUNT \
    ((int)(sizeof(Checkpoints) / sizeof(Checkpoints[0])))

static void BenchmarkConvergence(
    InteractiveOptimizer* optimizer,
    const TestProblem* problem,
    double* averages)
{
    double totals[CHECKPOINT_COUNT] = { 0.0 };

    for (int run = 0; run < RUNS; run++) {
        double bestX;
        double bestY;
        double value;
        int evaluations;

        optimizer->Init(problem);

        optimizer->GetBest(
            &bestX,
            &bestY,
            &value,
            &evaluations);

        int checkpointIndex = 0;

        while (checkpointIndex < CHECKPOINT_COUNT) {
            optimizer->Step();

            optimizer->GetBest(
                &bestX,
                &bestY,
                &value,
                &evaluations);

            while (
                checkpointIndex < CHECKPOINT_COUNT
                && evaluations >= Checkpoints[checkpointIndex]) {
                totals[checkpointIndex] += value;
                checkpointIndex++;
            }
        }
    }

    for (int i = 0; i < CHECKPOINT_COUNT; i++)
        averages[i] = totals[i] / RUNS;
}

static void PrintHeader(void)
{
    printf("%-24s", "Algorithm");

    for (int i = 0; i < CHECKPOINT_COUNT; i++)
        printf(" %12d", Checkpoints[i]);

    printf("\n");

    printf("%-24s", "------------------------");

    for (int i = 0; i < CHECKPOINT_COUNT; i++)
        printf(" %12s", "------------");

    printf("\n");
}

static void PrintResult(
    const char* name,
    const double* averages)
{
    printf("%-24s", name);

    for (int i = 0; i < CHECKPOINT_COUNT; i++)
        printf(" %12.6f", averages[i]);

    printf("\n");
}

int main(void)
{
    srand((unsigned int)time(NULL));

    TestProblem* problems[] = {
        &SphereProblem,
        &RastriginProblem,
        &RosenbrockProblem,
        &AckleyProblem,
        &HimmelblauProblem,
        &GriewankProblem
    };

    int problemCount = sizeof(problems) / sizeof(problems[0]);

    printf("Runs per algorithm: %d\n", RUNS);
    printf("Metric: average best value reached\n\n");

    for (int p = 0; p < problemCount; p++) {
        printf("%s\n\n", problems[p]->name);

        PrintHeader();

        for (int i = 0; i < OptimizerCount; i++) {
            double averages[CHECKPOINT_COUNT];

            BenchmarkConvergence(
                Optimizers[i],
                problems[p],
                averages);

            PrintResult(
                Optimizers[i]->name,
                averages);
        }

        printf("\n");
    }

    return 0;
}
