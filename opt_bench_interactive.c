#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ScoutHQInteractive.h"
#include "optimizers.h"
#include "problems.h"

#define RUNS 200
#define TIME_RUNS 100
#define SUCCESS_THRESHOLD 1e-6

static const int Checkpoints[] = {100, 300, 1000, 3000, 10000, 30000, 100000};
static const double TimeCheckpointsMs[] = {0.01, 0.03, 0.1, 0.3, 1.0, 3.0, 10.0, 30.0, 100.0, 300.0};
#define CHECKPOINT_COUNT ((int)(sizeof(Checkpoints) / sizeof(Checkpoints[0])))
#define TIME_CHECKPOINT_COUNT ((int)(sizeof(TimeCheckpointsMs) / sizeof(TimeCheckpointsMs[0])))

typedef struct
{
    double mean;
    double median;
    double best;
    double worst;
    double successPercent;
} FinalStatistics;

typedef struct
{
    double medianSuccessMs;
    double successPercent;
    double evaluationsPerSecond;
    int successes;
} TimeStatistics;

static double GetTimeMs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static int CompareDouble(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    return (da > db) - (da < db);
}

static void CalculateFinalStatistics(double *values, int count,
                                     const TestProblem *problem,
                                     FinalStatistics *statistics)
{
    double sum = 0.0;
    double best = INFINITY;
    double worst = -INFINITY;
    int successCount = 0;

    for (int i = 0; i < count; i++)
    {
        sum += values[i];
        if (values[i] < best)
            best = values[i];
        if (values[i] > worst)
            worst = values[i];
        if (fabs(values[i] - problem->optimumValue) < SUCCESS_THRESHOLD)
            successCount++;
    }

    qsort(values, count, sizeof(double), CompareDouble);
    double median = count % 2 == 0
                        ? (values[count / 2 - 1] + values[count / 2]) * 0.5
                        : values[count / 2];

    statistics->mean = sum / count;
    statistics->median = median;
    statistics->best = best;
    statistics->worst = worst;
    statistics->successPercent = 100.0 * successCount / count;
}

static void BenchmarkConvergence(InteractiveOptimizer *optimizer,
                                 const TestProblem *problem, double *averages,
                                 FinalStatistics *finalStatistics,
                                 int diagnostics)
{
    double totals[CHECKPOINT_COUNT] = {0.0};
    double finalValues[RUNS];

    for (int run = 0; run < RUNS; run++)
    {
        double bestX, bestY, value;
        int evaluations;
        optimizer->Init(problem);
        optimizer->GetBest(&bestX, &bestY, &value, &evaluations);
        int checkpointIndex = 0;

        while (checkpointIndex < CHECKPOINT_COUNT)
        {
            optimizer->Step();
            optimizer->GetBest(&bestX, &bestY, &value, &evaluations);
            while (checkpointIndex < CHECKPOINT_COUNT &&
                   evaluations >= Checkpoints[checkpointIndex])
            {
                totals[checkpointIndex] += value;
                if (checkpointIndex == CHECKPOINT_COUNT - 1)
                {
                    finalValues[run] = value;
                    if (diagnostics && optimizer == &ScoutHQOptimizer &&
                        problem == &GriewankProblem &&
                        fabs(value - problem->optimumValue) >= SUCCESS_THRESHOLD)
                    {
                        ScoutHQDiagnostics info;
                        ScoutHQGetDiagnostics(&info);
                        printf("\nScoutHQ Griewank miss %3d:\n", run + 1);
                        printf("  value=% .9e x=% .9e y=% .9e\n", value, bestX, bestY);
                        printf("  minStep=% .9e refinements=%d coverage=%d\n",
                               info.minimumStep, info.refinementCount,
                               info.coverageComplete);
                        for (int a = 0; a < 8; a++)
                        {
                            printf("  Agent %d: "
                                   "closest=% .9e "
                                   "closestValue=% .9e "
                                   "eval=%d "
                                   "step=% .9e "
                                   "best=% .9e "
                                   "teleports=%d "
                                   "refinements=%d\n",
                                   a, info.agents[a].closestOrigin,
                                   info.agents[a].closestOriginValue,
                                   info.agents[a].closestOriginEvaluation,
                                   info.agents[a].closestOriginStepSize,
                                   info.agents[a].bestValue,
                                   info.agents[a].teleportCount,
                                   info.agents[a].refinementCount);
                        }
                        printf("\n");
                    }
                }
                checkpointIndex++;
            }
        }
    }

    for (int i = 0; i < CHECKPOINT_COUNT; i++)
        averages[i] = totals[i] / RUNS;
    CalculateFinalStatistics(finalValues, RUNS, problem, finalStatistics);
}

static void BenchmarkTime(InteractiveOptimizer *optimizer,
                          const TestProblem *problem, double *averages,
                          TimeStatistics *statistics)
{
    double totals[TIME_CHECKPOINT_COUNT] = {0.0};
    double successTimes[TIME_RUNS];
    int successes = 0;
    double totalEvaluations = 0.0;
    double totalElapsedMs = 0.0;

    for (int run = 0; run < TIME_RUNS; run++)
    {
        double bestX, bestY, value;
        int evaluations;
        optimizer->Init(problem);
        optimizer->GetBest(&bestX, &bestY, &value, &evaluations);
        int initialEvaluations = evaluations;
        int successRecorded = 0;
        int checkpointIndex = 0;
        double startTime = GetTimeMs();

        if (fabs(value - problem->optimumValue) < SUCCESS_THRESHOLD)
        {
            successTimes[successes++] = 0.0;
            successRecorded = 1;
        }

        while (checkpointIndex < TIME_CHECKPOINT_COUNT)
        {
            optimizer->Step();
            optimizer->GetBest(&bestX, &bestY, &value, &evaluations);
            double elapsedMs = GetTimeMs() - startTime;

            if (!successRecorded &&
                fabs(value - problem->optimumValue) < SUCCESS_THRESHOLD)
            {
                successTimes[successes++] = elapsedMs;
                successRecorded = 1;
            }

            while (checkpointIndex < TIME_CHECKPOINT_COUNT &&
                   elapsedMs >= TimeCheckpointsMs[checkpointIndex])
            {
                totals[checkpointIndex] += value;
                checkpointIndex++;
            }
        }

        double elapsedMs = GetTimeMs() - startTime;
        totalElapsedMs += elapsedMs;
        totalEvaluations += (double)(evaluations - initialEvaluations);
    }

    for (int i = 0; i < TIME_CHECKPOINT_COUNT; i++)
        averages[i] = totals[i] / TIME_RUNS;

    statistics->successes = successes;
    statistics->successPercent = 100.0 * successes / TIME_RUNS;
    statistics->evaluationsPerSecond = totalElapsedMs > 0.0
        ? totalEvaluations * 1000.0 / totalElapsedMs : 0.0;
    statistics->medianSuccessMs = NAN;
    if (successes > 0)
    {
        qsort(successTimes, successes, sizeof(double), CompareDouble);
        statistics->medianSuccessMs = successes % 2 == 0
            ? (successTimes[successes / 2 - 1] + successTimes[successes / 2]) * 0.5
            : successTimes[successes / 2];
    }
}

static void PrintEvaluationHeader(void)
{
    printf("%-24s", "Algorithm");
    for (int i = 0; i < CHECKPOINT_COUNT; i++)
        printf(" %12d", Checkpoints[i]);
    printf("\n%-24s", "------------------------");
    for (int i = 0; i < CHECKPOINT_COUNT; i++)
        printf(" %12s", "------------");
    printf("\n");
}

static void PrintTimeHeader(void)
{
    printf("%-24s", "Algorithm");
    for (int i = 0; i < TIME_CHECKPOINT_COUNT; i++)
        printf(" %12.2f", TimeCheckpointsMs[i]);
    printf("\n%-24s", "------------------------");
    for (int i = 0; i < TIME_CHECKPOINT_COUNT; i++)
        printf(" %12s", "------------");
    printf("\n");
}

static void PrintFinalStatisticsHeader(void)
{
    printf("%-24s %12s %12s %12s %12s %10s\n", "Algorithm", "Mean", "Median",
           "Best", "Worst", "Success");
    printf("%-24s %12s %12s %12s %12s %10s\n", "------------------------",
           "------------", "------------", "------------", "------------",
           "----------");
}

static void PrintEvaluationResult(const char *name, const double *averages)
{
    printf("%-24s", name);
    for (int i = 0; i < CHECKPOINT_COUNT; i++)
        printf(" %12.6f", averages[i]);
    printf("\n");
}

static void PrintTimeResult(const char *name, const double *averages)
{
    printf("%-24s", name);
    for (int i = 0; i < TIME_CHECKPOINT_COUNT; i++)
        printf(" %12.5g", averages[i]);
    printf("\n");
}

static void PrintFinalStatistics(const char *name,
                                 const FinalStatistics *statistics)
{
    printf("%-24s %12.6f %12.6f %12.6f %12.6f %9.1f%%\n", name,
           statistics->mean, statistics->median, statistics->best,
           statistics->worst, statistics->successPercent);
}

static void PrintTimeStatisticsHeader(void)
{
    printf("%-24s %18s %12s %16s\n", "Algorithm", "Median hit (ms)",
           "Success", "Eval/s");
    printf("%-24s %18s %12s %16s\n", "------------------------",
           "------------------", "------------", "----------------");
}

static void PrintTimeStatistics(const char *name,
                                const TimeStatistics *statistics)
{
    printf("%-24s ", name);
    if (statistics->successes == 0)
        printf("%18s", "N/A");
    else
        printf("%18.6f", statistics->medianSuccessMs);
    printf(" %11.1f%% %16.0f\n", statistics->successPercent,
           statistics->evaluationsPerSecond);
}

int main(int argc, char *argv[])
{
    int runTimeBenchmark = 0;
    int diagnostics = 0;
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-t") == 0)
            runTimeBenchmark = 1;
        if (strcmp(argv[i], "-d") == 0)
            diagnostics = 1;
    }

    srand((unsigned int)time(NULL));
    TestProblem *problems[] = {&SphereProblem, &RastriginProblem,
                               &RosenbrockProblem, &AckleyProblem,
                               &HimmelblauProblem, &GriewankProblem};
    int problemCount = sizeof(problems) / sizeof(problems[0]);

    printf("Evaluation benchmark\n");
    printf("Runs per algorithm: %d\n", RUNS);
    printf("Metric: average best value reached\n\n");

    for (int p = 0; p < problemCount; p++)
    {
        printf("%s\n\n", problems[p]->name);
        PrintEvaluationHeader();
        FinalStatistics statistics[OptimizerCount];
        for (int i = 0; i < OptimizerCount; i++)
        {
            double averages[CHECKPOINT_COUNT];
            BenchmarkConvergence(Optimizers[i], problems[p], averages,
                                 &statistics[i], diagnostics);
            PrintEvaluationResult(Optimizers[i]->name, averages);
        }
        printf("\nFinal statistics at %d evaluations\n",
               Checkpoints[CHECKPOINT_COUNT - 1]);
        printf("Success threshold: |value - optimum| < %.1e\n\n",
               SUCCESS_THRESHOLD);
        PrintFinalStatisticsHeader();
        for (int i = 0; i < OptimizerCount; i++)
            PrintFinalStatistics(Optimizers[i]->name, &statistics[i]);
        printf("\n");
    }

    if (runTimeBenchmark)
    {
        printf("\nTime benchmark\n");
        printf("Runs per algorithm: %d\n", TIME_RUNS);
        printf("Metric: average best value reached\n");
        printf("Time checkpoints in milliseconds\n");
        printf("Median hit time is for successful runs only; N/A means no successes.\n");
        printf("Eval/s counts evaluations after Init, during timed steps.\n\n");

        for (int p = 0; p < problemCount; p++)
        {
            printf("%s\n\n", problems[p]->name);
            PrintTimeHeader();
            TimeStatistics statistics[OptimizerCount];
            for (int i = 0; i < OptimizerCount; i++)
            {
                double averages[TIME_CHECKPOINT_COUNT];
                BenchmarkTime(Optimizers[i], problems[p], averages,
                              &statistics[i]);
                PrintTimeResult(Optimizers[i]->name, averages);
            }
            printf("\nTime-to-convergence and throughput (%.1e threshold)\n",
                   SUCCESS_THRESHOLD);
            PrintTimeStatisticsHeader();
            for (int i = 0; i < OptimizerCount; i++)
                PrintTimeStatistics(Optimizers[i]->name, &statistics[i]);
            printf("\n");
        }
    }
    return 0;
}
