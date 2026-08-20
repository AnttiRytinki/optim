#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "optimizers.h"
#include "problems.h"

#define RUNS 200
#define TIME_RUNS 10

#define SUCCESS_THRESHOLD 1e-6

static const int Checkpoints[] = {
    100,
    300,
    1000,
    3000,
    10000,
    30000,
    100000
};

static const double TimeCheckpointsMs[] = {
    1.0,
    3.0,
    10.0,
    30.0,
    100.0,
    300.0
};

#define CHECKPOINT_COUNT \
    ((int)(sizeof(Checkpoints) / sizeof(Checkpoints[0])))

#define TIME_CHECKPOINT_COUNT \
    ((int)(sizeof(TimeCheckpointsMs) / sizeof(TimeCheckpointsMs[0])))

typedef struct
{
    double mean;
    double median;
    double best;
    double worst;
    double successPercent;
} FinalStatistics;

static double GetTimeMs(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (double)ts.tv_sec * 1000.0
        + (double)ts.tv_nsec / 1000000.0;
}

static int CompareDouble(const void* a, const void* b)
{
    double da = *(const double*)a;
    double db = *(const double*)b;

    if (da < db)
        return -1;

    if (da > db)
        return 1;

    return 0;
}

static void CalculateFinalStatistics(
    double* values,
    int count,
    const TestProblem* problem,
    FinalStatistics* statistics)
{
    double sum = 0.0;
    double best = INFINITY;
    double worst = -INFINITY;
    int successCount = 0;

    for (int i = 0; i < count; i++) {
        sum += values[i];

        if (values[i] < best)
            best = values[i];

        if (values[i] > worst)
            worst = values[i];

        if (
            fabs(values[i] - problem->optimumValue)
            < SUCCESS_THRESHOLD) {
            successCount++;
        }
    }

    qsort(
        values,
        count,
        sizeof(double),
        CompareDouble);

    double median;

    if (count % 2 == 0) {
        median = (values[count / 2 - 1]
                     + values[count / 2])
            * 0.5;
    } else
        median = values[count / 2];

    statistics->mean = sum / count;

    statistics->median = median;

    statistics->best = best;

    statistics->worst = worst;

    statistics->successPercent = 100.0
        * successCount
        / count;
}

static void BenchmarkConvergence(
    InteractiveOptimizer* optimizer,
    const TestProblem* problem,
    double* averages,
    FinalStatistics* finalStatistics)
{
    double totals[CHECKPOINT_COUNT] = { 0.0 };
    double finalValues[RUNS];

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

                if (checkpointIndex == CHECKPOINT_COUNT - 1)
                    finalValues[run] = value;

                checkpointIndex++;
            }
        }
    }

    for (int i = 0; i < CHECKPOINT_COUNT; i++)
        averages[i] = totals[i] / RUNS;

    CalculateFinalStatistics(
        finalValues,
        RUNS,
        problem,
        finalStatistics);
}

static void BenchmarkTime(
    InteractiveOptimizer* optimizer,
    const TestProblem* problem,
    double* averages)
{
    double totals[TIME_CHECKPOINT_COUNT] = { 0.0 };

    for (int run = 0; run < TIME_RUNS; run++) {
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

        double startTime = GetTimeMs();

        int checkpointIndex = 0;

        while (checkpointIndex < TIME_CHECKPOINT_COUNT) {
            optimizer->Step();

            optimizer->GetBest(
                &bestX,
                &bestY,
                &value,
                &evaluations);

            double elapsedMs = GetTimeMs() - startTime;

            while (
                checkpointIndex < TIME_CHECKPOINT_COUNT
                && elapsedMs >= TimeCheckpointsMs[checkpointIndex]) {
                totals[checkpointIndex] += value;
                checkpointIndex++;
            }
        }
    }

    for (int i = 0; i < TIME_CHECKPOINT_COUNT; i++)
        averages[i] = totals[i] / TIME_RUNS;
}

static void PrintEvaluationHeader(void)
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

static void PrintTimeHeader(void)
{
    printf("%-24s", "Algorithm");

    for (int i = 0; i < TIME_CHECKPOINT_COUNT; i++)
        printf(" %12.0f", TimeCheckpointsMs[i]);

    printf("\n");

    printf("%-24s", "------------------------");

    for (int i = 0; i < TIME_CHECKPOINT_COUNT; i++)
        printf(" %12s", "------------");

    printf("\n");
}

static void PrintFinalStatisticsHeader(void)
{
    printf(
        "%-24s %12s %12s %12s %12s %10s\n",
        "Algorithm",
        "Mean",
        "Median",
        "Best",
        "Worst",
        "Success");

    printf(
        "%-24s %12s %12s %12s %12s %10s\n",
        "------------------------",
        "------------",
        "------------",
        "------------",
        "------------",
        "----------");
}

static void PrintEvaluationResult(
    const char* name,
    const double* averages)
{
    printf("%-24s", name);

    for (int i = 0; i < CHECKPOINT_COUNT; i++)
        printf(" %12.6f", averages[i]);

    printf("\n");
}

static void PrintTimeResult(
    const char* name,
    const double* averages)
{
    printf("%-24s", name);

    for (int i = 0; i < TIME_CHECKPOINT_COUNT; i++)
        printf(" %12.6f", averages[i]);

    printf("\n");
}

static void PrintFinalStatistics(
    const char* name,
    const FinalStatistics* statistics)
{
    printf(
        "%-24s %12.6f %12.6f %12.6f %12.6f %9.1f%%\n",
        name,
        statistics->mean,
        statistics->median,
        statistics->best,
        statistics->worst,
        statistics->successPercent);
}

int main(int argc, char* argv[])
{
    int runTimeBenchmark = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0)
            runTimeBenchmark = 1;
    }

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

    printf("Evaluation benchmark\n");
    printf("Runs per algorithm: %d\n", RUNS);
    printf("Metric: average best value reached\n\n");

    for (int p = 0; p < problemCount; p++) {
        printf("%s\n\n", problems[p]->name);

        PrintEvaluationHeader();

        FinalStatistics statistics[OptimizerCount];

        for (int i = 0; i < OptimizerCount; i++) {
            double averages[CHECKPOINT_COUNT];

            BenchmarkConvergence(
                Optimizers[i],
                problems[p],
                averages,
                &statistics[i]);

            PrintEvaluationResult(
                Optimizers[i]->name,
                averages);
        }

        printf("\n");
        printf(
            "Final statistics at %d evaluations\n",
            Checkpoints[CHECKPOINT_COUNT - 1]);

        printf(
            "Success threshold: |value - optimum| < %.1e\n\n",
            SUCCESS_THRESHOLD);

        PrintFinalStatisticsHeader();

        for (int i = 0; i < OptimizerCount; i++) {
            PrintFinalStatistics(
                Optimizers[i]->name,
                &statistics[i]);
        }

        printf("\n");
    }

    if (runTimeBenchmark) {
        printf("\n");
        printf("Time benchmark\n");
        printf("Runs per algorithm: %d\n", TIME_RUNS);
        printf("Metric: average best value reached\n");
        printf("Time checkpoints in milliseconds\n\n");

        for (int p = 0; p < problemCount; p++) {
            printf("%s\n\n", problems[p]->name);

            PrintTimeHeader();

            for (int i = 0; i < OptimizerCount; i++) {
                double averages[TIME_CHECKPOINT_COUNT];

                BenchmarkTime(
                    Optimizers[i],
                    problems[p],
                    averages);

                PrintTimeResult(
                    Optimizers[i]->name,
                    averages);
            }

            printf("\n");
        }
    }

    return 0;
}
