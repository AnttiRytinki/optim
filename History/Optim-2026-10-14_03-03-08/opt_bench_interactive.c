#define _POSIX_C_SOURCE 200809L
#include "ScoutHQInteractive.h"
#include "optimizers.h"
#include "problems.h"
#ifdef OPTIM_WITH_NOMAD
#include "nomad_adapter.h"
extern InteractiveOptimizer NomadOptimizer;
#endif
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RUNS 200
#define TIME_RUNS 100
#define SUCCESS_THRESHOLD 1e-6

static const int Checkpoints[] = {100, 300, 1000, 3000, 10000, 30000, 100000};
static const double TimeCheckpointsMs[] = {0.01, 0.03, 0.1,  0.3,   1.0,
                                           3.0,  10.0, 30.0, 100.0, 300.0};
#define CHECKPOINT_COUNT ((int)(sizeof(Checkpoints) / sizeof(Checkpoints[0])))
#define TIME_CHECKPOINT_COUNT                                                  \
  ((int)(sizeof(TimeCheckpointsMs) / sizeof(TimeCheckpointsMs[0])))

typedef struct {
  double mean;
  double median;
  double best;
  double worst;
  double successPercent;
} FinalStatistics;

typedef struct {
  double medianSuccessMs;
  double successPercent;
  double evaluationsPerSecond;
  int successes;
} TimeStatistics;

static int runCount = RUNS;
static int timeRunCount = TIME_RUNS;
static int runOffset;
static unsigned int baseSeed = 20261014u;
static int problemIndex;
static FILE *detailFile;
static ObjectiveFunction measuredFunction;
static double measuredBest, measuredStart, measuredHit, measuredOptimum;
static double measuredValues[CHECKPOINT_COUNT];
static double measuredTimes[TIME_CHECKPOINT_COUNT];
static double measuredFirst[MAX_DIM];
static int measuredCalls, measuredTimed, measuredTimeIndex, measuredWindowCalls;
static int measuredSuccessEvaluation;

static unsigned int RunSeed(int run)
{
    return (unsigned int)(((uint64_t)baseSeed + 1000003ull * problemIndex +
                           7919ull * (run + runOffset)) % 2147483647ull);
}

static double GetTimeMs(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}

static int CompareDouble(const void *a, const void *b) {
  double da = *(const double *)a;
  double db = *(const double *)b;
  return (da > db) - (da < db);
}

static void CalculateFinalStatistics(double *values, int count,
                                     const TestProblem *problem,
                                     FinalStatistics *statistics) {
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

/* Instrumentation sees every actual call, including Init and batched Steps. */
static double MeasuredObjective(const double *x, int dim)
{
    double value = measuredFunction(x, dim);
    double elapsed = measuredTimed ? GetTimeMs() - measuredStart : 0.0;
    if (measuredTimed)
    {
        while (measuredTimeIndex < TIME_CHECKPOINT_COUNT &&
               elapsed > TimeCheckpointsMs[measuredTimeIndex])
        {
            measuredTimes[measuredTimeIndex++] = measuredBest;
        }
    }
    measuredBest = fmin(measuredBest, value);
    if (measuredCalls == 0)
    {
        memcpy(measuredFirst, x, (size_t)dim * sizeof(double));
    }
    ++measuredCalls;
    if (!measuredSuccessEvaluation &&
        fabs(measuredBest - measuredOptimum) < SUCCESS_THRESHOLD)
    {
        measuredSuccessEvaluation = measuredCalls;
    }
    if (measuredTimed)
    {
        if (elapsed <= TimeCheckpointsMs[TIME_CHECKPOINT_COUNT - 1])
        {
            measuredWindowCalls++;
            if (isnan(measuredHit) && fabs(value - measuredOptimum) < SUCCESS_THRESHOLD)
            {
                measuredHit = elapsed;
            }
        }
    }
    else
    {
        for (int i = 0; i < CHECKPOINT_COUNT; ++i)
        {
            if (measuredCalls == Checkpoints[i])
            {
                measuredValues[i] = measuredBest;
            }
        }
    }
    return value;
}

static void BeginMeasurement(const TestProblem *problem, int timed)
{
    measuredFunction = problem->function;
    measuredOptimum = problem->optimumValue;
    measuredBest = INFINITY;
    measuredHit = NAN;
    measuredCalls = measuredWindowCalls = measuredTimeIndex = 0;
    measuredSuccessEvaluation = 0;
    measuredTimed = timed;
    for (int i = 0; i < CHECKPOINT_COUNT; ++i)
    {
        measuredValues[i] = NAN;
    }
    measuredStart = GetTimeMs();
}

#ifdef OPTIM_WITH_NOMAD
static NomadRun *RunNomad(const TestProblem *problem, int run, double *preparationMs)
{
    double begin = GetTimeMs();
    unsigned int seed = RunSeed(run);
    srand(seed);
    double start[MAX_DIM] = {0};
    for (int d = 0; d < problem->dim; ++d)
    {
        start[d] = RandomDouble(problem->lower, problem->upper);
    }
    if (preparationMs)
    {
        *preparationMs = GetTimeMs() - begin;
    }
    char error[1024];
    NomadRun *result = NomadOptimize(problem, start, (int)seed,
                                     Checkpoints[CHECKPOINT_COUNT - 1],
                                     error, sizeof(error));
    if (!result)
    {
        fprintf(stderr, "NOMAD %s run %d failed: %s\n", problem->name, run + 1, error);
        exit(1);
    }
    return result;
}
#endif

static void BenchmarkConvergence(InteractiveOptimizer *optimizer,
                                 const TestProblem *problem, double *averages,
                                 FinalStatistics *finalStatistics,
                                 int diagnostics)
{
    double totals[CHECKPOINT_COUNT] = {0};
    int successes[CHECKPOINT_COUNT] = {0};
    double finalValues[runCount];
    int minimumCount = INT_MAX, maximumCount = 0;
    double totalCount = 0;
    for (int run = 0; run < runCount; ++run)
    {
        double values[CHECKPOINT_COUNT];
        int count;
        double firstX, firstY;
        int firstSuccess = 0;
#ifdef OPTIM_WITH_NOMAD
        if (optimizer == &NomadOptimizer)
        {
            NomadRun *result = RunNomad(problem, run, NULL);
            count = (int)NomadEvaluationCount(result);
            firstX = NomadEvaluationAt(result, 0)->point[0];
            firstY = NomadEvaluationAt(result, 0)->point[1];
            for (int i = 0; i < CHECKPOINT_COUNT; ++i)
            {
                values[i] = NomadAtBudget(result, Checkpoints[i])->bestValue;
            }
            for (size_t i = 0; i < NomadEvaluationCount(result); ++i)
            {
                if (fabs(NomadEvaluationAt(result, i)->bestValue - problem->optimumValue) < SUCCESS_THRESHOLD)
                {
                    firstSuccess = (int)i + 1;
                    break;
                }
            }
            NomadDestroy(result);
        }
        else
#endif
        {
            srand(RunSeed(run));
            TestProblem instrumented = *problem;
            instrumented.function = MeasuredObjective;
            BeginMeasurement(problem, 0);
            optimizer->Init(&instrumented);
            while (measuredCalls < Checkpoints[CHECKPOINT_COUNT - 1])
            {
                optimizer->Step();
            }
            double x, y, value;
            int reported;
            optimizer->GetBest(&x, &y, &value, &reported);
            if (reported != measuredCalls)
            {
                fprintf(stderr, "%s evaluation count mismatch\n", optimizer->name);
                exit(1);
            }
          if (diagnostics && optimizer == &ScoutHQOptimizer &&
              problem == &GriewankProblem &&
              fabs(measuredValues[CHECKPOINT_COUNT - 1] - problem->optimumValue) >= SUCCESS_THRESHOLD) {
            ScoutHQDiagnostics info;
            ScoutHQGetDiagnostics(&info);
            printf("\nScoutHQ Griewank miss %3d:\n", run + 1);
            printf("  value=% .9e x=% .9e y=% .9e\n", value, x, y);
            printf("  minStep=% .9e refinements=%d coverage=%d\n",
                   info.minimumStep, info.refinementCount,
                   info.coverageComplete);
            for (int a = 0; a < 8; a++) {
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
                     info.agents[a].bestValue, info.agents[a].teleportCount,
                     info.agents[a].refinementCount);
            }
            printf("\n");
          }
            count = measuredCalls;
            firstX = measuredFirst[0];
            firstY = measuredFirst[1];
            firstSuccess = measuredSuccessEvaluation <= Checkpoints[CHECKPOINT_COUNT - 1]
                ? measuredSuccessEvaluation : 0;
            for (int i = 0; i < CHECKPOINT_COUNT; ++i)
            {
                values[i] = measuredValues[i];
            }
        }
        minimumCount = count < minimumCount ? count : minimumCount;
        maximumCount = count > maximumCount ? count : maximumCount;
        totalCount += count;
        for (int i = 0; i < CHECKPOINT_COUNT; ++i)
        {
            totals[i] += values[i];
            successes[i] += fabs(values[i] - problem->optimumValue) < SUCCESS_THRESHOLD;
            if (detailFile)
            {
                fprintf(detailFile, "%s,%s,%d,%u,%d,%.17g,%d,%.17g,%.17g,%d\n", problem->name,
                        optimizer->name, run + runOffset + 1, RunSeed(run), Checkpoints[i], values[i], count, firstX, firstY, firstSuccess);
            }
        }
        finalValues[run] = values[CHECKPOINT_COUNT - 1];
    }
    for (int i = 0; i < CHECKPOINT_COUNT; ++i)
    {
        averages[i] = totals[i] / runCount;
    }
    CalculateFinalStatistics(finalValues, runCount, problem, finalStatistics);
    printf("  %s actual calls: min=%d mean=%.1f max=%d (includes any Step overshoot)\n",
           optimizer->name, minimumCount, totalCount / runCount, maximumCount);
    printf("  Success by checkpoint:");
    for (int i = 0; i < CHECKPOINT_COUNT; ++i)
    {
        printf(" %d=%.1f%%", Checkpoints[i], 100.0 * successes[i] / runCount);
    }
    printf("\n");
}

static void BenchmarkTime(InteractiveOptimizer *optimizer,
                          const TestProblem *problem, double *averages,
                          TimeStatistics *statistics)
{
    double totals[TIME_CHECKPOINT_COUNT] = {0};
    double successTimes[timeRunCount];
    int successes = 0;
    double totalEvaluations = 0;
    for (int run = 0; run < timeRunCount; ++run)
    {
#ifdef OPTIM_WITH_NOMAD
        if (optimizer == &NomadOptimizer)
        {
            double preparationMs;
            NomadRun *result = RunNomad(problem, run, &preparationMs);
            for (int i = 0; i < TIME_CHECKPOINT_COUNT; ++i)
            {
                const NomadEvaluation *entry = NomadAtTime(result, TimeCheckpointsMs[i] - preparationMs);
                totals[i] += entry ? entry->bestValue : INFINITY;
            }
            int hit = 0;
            for (size_t i = 0; i < NomadEvaluationCount(result); ++i)
            {
                const NomadEvaluation *entry = NomadEvaluationAt(result, i);
                if (entry->elapsedMs + preparationMs > TimeCheckpointsMs[TIME_CHECKPOINT_COUNT - 1])
                {
                    break;
                }
                totalEvaluations++;
                if (!hit && fabs(entry->bestValue - problem->optimumValue) < SUCCESS_THRESHOLD)
                {
                    successTimes[successes++] = entry->elapsedMs + preparationMs;
                    hit = 1;
                }
            }
            NomadDestroy(result);
        }
        else
#endif
        {
            srand(RunSeed(run));
            TestProblem instrumented = *problem;
            instrumented.function = MeasuredObjective;
            BeginMeasurement(problem, 1);
            optimizer->Init(&instrumented);
            while (GetTimeMs() - measuredStart < TimeCheckpointsMs[TIME_CHECKPOINT_COUNT - 1])
            {
                optimizer->Step();
            }
            while (measuredTimeIndex < TIME_CHECKPOINT_COUNT)
            {
                measuredTimes[measuredTimeIndex++] = measuredBest;
            }
            for (int i = 0; i < TIME_CHECKPOINT_COUNT; ++i)
            {
                totals[i] += measuredTimes[i];
            }
            if (!isnan(measuredHit))
            {
                successTimes[successes++] = measuredHit;
            }
            totalEvaluations += measuredWindowCalls;
        }
    }
    for (int i = 0; i < TIME_CHECKPOINT_COUNT; ++i)
    {
        averages[i] = totals[i] / timeRunCount;
    }
    statistics->successes = successes;
    statistics->successPercent = 100.0 * successes / timeRunCount;
    statistics->evaluationsPerSecond = totalEvaluations * 1000.0 /
        (timeRunCount * TimeCheckpointsMs[TIME_CHECKPOINT_COUNT - 1]);
    statistics->medianSuccessMs = NAN;
    if (successes)
    {
        qsort(successTimes, successes, sizeof(double), CompareDouble);
        statistics->medianSuccessMs = successes % 2 ? successTimes[successes / 2] :
            (successTimes[successes / 2 - 1] + successTimes[successes / 2]) / 2;
    }
}

static void PrintEvaluationHeader(void) {
  printf("%-24s", "Algorithm");
  for (int i = 0; i < CHECKPOINT_COUNT; i++)
    printf(" %12d", Checkpoints[i]);
  printf("\n%-24s", "------------------------");
  for (int i = 0; i < CHECKPOINT_COUNT; i++)
    printf(" %12s", "------------");
  printf("\n");
}

static void PrintTimeHeader(void) {
  printf("%-24s", "Algorithm");
  for (int i = 0; i < TIME_CHECKPOINT_COUNT; i++)
    printf(" %12.2f", TimeCheckpointsMs[i]);
  printf("\n%-24s", "------------------------");
  for (int i = 0; i < TIME_CHECKPOINT_COUNT; i++)
    printf(" %12s", "------------");
  printf("\n");
}

static void PrintFinalStatisticsHeader(void) {
  printf("%-24s %12s %12s %12s %12s %10s\n", "Algorithm", "Mean", "Median",
         "Best", "Worst", "Success");
  printf("%-24s %12s %12s %12s %12s %10s\n", "------------------------",
         "------------", "------------", "------------", "------------",
         "----------");
}

static void PrintEvaluationResult(const char *name, const double *averages) {
  printf("%-24s", name);
  for (int i = 0; i < CHECKPOINT_COUNT; i++)
    printf(" %12.5e", averages[i]);
  printf("\n");
}

static void PrintTimeResult(const char *name, const double *averages) {
  printf("%-24s", name);
  for (int i = 0; i < TIME_CHECKPOINT_COUNT; i++)
    printf(" %12.5g", averages[i]);
  printf("\n");
}

static void PrintFinalStatistics(const char *name,
                                 const FinalStatistics *statistics) {
  printf("%-24s %12.5e %12.5e %12.5e %12.5e %9.1f%%\n", name, statistics->mean,
         statistics->median, statistics->best, statistics->worst,
         statistics->successPercent);
}

static void PrintTimeStatisticsHeader(void) {
  printf("%-24s %18s %12s %16s\n", "Algorithm", "Median hit (ms)", "Success",
         "Eval/s");
  printf("%-24s %18s %12s %16s\n", "------------------------",
         "------------------", "------------", "----------------");
}

static void PrintTimeStatistics(const char *name,
                                const TimeStatistics *statistics) {
  printf("%-24s ", name);
  if (statistics->successes == 0)
    printf("%18s", "N/A");
  else
    printf("%18.6f", statistics->medianSuccessMs);
  printf(" %11.1f%% %16.0f\n", statistics->successPercent,
         statistics->evaluationsPerSecond);
}

static int LoadOptimizerList(const char *filename, int *selected) {
  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    perror(filename);
    return 0;
  }

  char line[256];
  int lineNumber = 0;
  int selectedCount = 0;
  while (fgets(line, sizeof(line), file) != NULL) {
    lineNumber++;
    if (strchr(line, '\n') == NULL && !feof(file)) {
      fprintf(stderr, "%s:%d: line too long\n", filename, lineNumber);
      fclose(file);
      return 0;
    }
    char *start = line;
    while (*start == ' ' || *start == '\t' || *start == '\r')
      start++;
    char *end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' ||
                           end[-1] == '\r' || end[-1] == '\n'))
      *--end = '\0';
    if (*start == '\0' || *start == '#')
      continue;

    int found = 0;
    for (int i = 0; i < OptimizerCount; i++) {
      if (strcmp(start, Optimizers[i]->name) == 0) {
        if (!selected[i]) {
          selected[i] = 1;
          selectedCount++;
        }
        found = 1;
        break;
      }
    }
    if (!found) {
      fprintf(stderr, "%s:%d: unknown optimizer '%s'\n", filename, lineNumber,
              start);
      fclose(file);
      return 0;
    }
  }
  if (ferror(file)) {
    perror(filename);
    fclose(file);
    return 0;
  }
  fclose(file);
  if (selectedCount == 0) {
    fprintf(stderr, "%s: no optimizers selected\n", filename);
    return 0;
  }
  return 1;
}

int main(int argc, char *argv[]) {
  setvbuf(stdout, NULL, _IOLBF, 0);
  int runTimeBenchmark = 0;
  int diagnostics = 0;
  const char *optimizerListFile = NULL;
  int selected[OptimizerCount];
  for (int i = 0; i < OptimizerCount; i++)
    selected[i] = 1;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-seed") == 0 || strcmp(argv[i], "-runs") == 0)
    {
      int seeds = strcmp(argv[i], "-seed") == 0;
      if (++i >= argc)
        return 1;
      char *end;
      unsigned long parsed = strtoul(argv[i], &end, 10);
      if (!argv[i][0] || argv[i][0] == '-' || *end || parsed > 2147483647ul ||
          (!seeds && (parsed == 0 || parsed > 10000)))
        return 1;
      if (seeds)
        baseSeed = (unsigned int)parsed;
      else
        runCount = (int)parsed;
    }
    else if (strcmp(argv[i], "-run-offset") == 0 || strcmp(argv[i], "-time-runs") == 0)
    {
      int offset = strcmp(argv[i], "-run-offset") == 0;
      if (++i >= argc)
        return 1;
      char *end;
      unsigned long parsed = strtoul(argv[i], &end, 10);
      if (!argv[i][0] || argv[i][0] == '-' || *end || parsed > 1000000ul ||
          (!offset && (parsed == 0 || parsed > 10000)))
        return 1;
      if (offset)
        runOffset = (int)parsed;
      else
        timeRunCount = (int)parsed;
    }
    else if (strcmp(argv[i], "-csv") == 0)
    {
      if (++i >= argc || detailFile)
        return 1;
      detailFile = fopen(argv[i], "w");
      if (!detailFile)
      {
        perror(argv[i]);
        return 1;
      }
      fprintf(detailFile, "problem,algorithm,run,seed,budget,best,actual_calls,start_x,start_y,first_success_eval\n");
    }
    else if (strcmp(argv[i], "-t") == 0)
      runTimeBenchmark = 1;
    else if (strcmp(argv[i], "-d") == 0)
      diagnostics = 1;
    else if (strcmp(argv[i], "-optlist") == 0) {
      if (++i >= argc || optimizerListFile != NULL) {
        fprintf(stderr, "Usage: %s [-t] [-d] [-optlist filename] [-seed N] [-runs N] [-csv filename] [-run-offset N] [-time-runs N]\n", argv[0]);
        return 1;
      }
      optimizerListFile = argv[i];
    } else {
      fprintf(stderr, "Unknown argument: %s\n", argv[i]);
      return 1;
    }
  }
  if (optimizerListFile != NULL) {
    for (int i = 0; i < OptimizerCount; i++)
      selected[i] = 0;
    if (!LoadOptimizerList(optimizerListFile, selected))
      return 1;
  }


  TestProblem *problems[] = {&SphereProblem,     &RastriginProblem,
                             &RosenbrockProblem, &AckleyProblem,
                             &HimmelblauProblem, &GriewankProblem};
  int problemCount = sizeof(problems) / sizeof(problems[0]);

  printf("Evaluation benchmark\n");
  printf("Runs per algorithm: %d\n", runCount);
  printf("Base seed: %u; paired per-run seeds and ScoutsD/NOMAD starts\n", baseSeed);
  printf("Exact objective-call checkpoints; initialization included\n");
  printf("Metric: average best value reached\n\n");

  for (int p = 0; p < problemCount; p++) {
    problemIndex = p;
    printf("%s\n\n", problems[p]->name);
    PrintEvaluationHeader();
    FinalStatistics statistics[OptimizerCount];
    for (int i = 0; i < OptimizerCount; i++) {
      if (!selected[i])
        continue;
      double averages[CHECKPOINT_COUNT];
      BenchmarkConvergence(Optimizers[i], problems[p], averages, &statistics[i],
                           diagnostics);
      PrintEvaluationResult(Optimizers[i]->name, averages);
    }
    printf("\nFinal statistics at %d evaluations\n",
           Checkpoints[CHECKPOINT_COUNT - 1]);
    printf("Success threshold: |value - optimum| < %.1e\n\n",
           SUCCESS_THRESHOLD);
    PrintFinalStatisticsHeader();
    for (int i = 0; i < OptimizerCount; i++) {
      if (!selected[i])
        continue;
      PrintFinalStatistics(Optimizers[i]->name, &statistics[i]);
    }
    printf("\n");
  }

  if (runTimeBenchmark) {
    printf("\nTime benchmark\n");
    printf("Runs per algorithm: %d\n", timeRunCount);
    printf("Metric: average best value reached\n");
    printf("Time checkpoints in milliseconds\n");
    printf("Median hit time is for successful runs only; N/A means no "
           "successes.\n");
    printf("Timing includes Init; samples use the last completed objective at each deadline.\n");
    printf("Eval/s counts completed evaluations within 300 ms, including Init.\n");
    printf("NOMAD runs to its evaluation limit; recorded timestamps select time results.\n\n");

    for (int p = 0; p < problemCount; p++) {
      problemIndex = p;
      printf("%s\n\n", problems[p]->name);
      PrintTimeHeader();
      TimeStatistics statistics[OptimizerCount];
      for (int i = 0; i < OptimizerCount; i++) {
        if (!selected[i])
          continue;
        double averages[TIME_CHECKPOINT_COUNT];
        BenchmarkTime(Optimizers[i], problems[p], averages, &statistics[i]);
        PrintTimeResult(Optimizers[i]->name, averages);
      }
      printf("\nTime-to-convergence and throughput (%.1e threshold)\n",
             SUCCESS_THRESHOLD);
      PrintTimeStatisticsHeader();
      for (int i = 0; i < OptimizerCount; i++) {
        if (!selected[i])
          continue;
        PrintTimeStatistics(Optimizers[i]->name, &statistics[i]);
      }
      printf("\n");
    }
  }
  if (detailFile && fclose(detailFile) != 0)
    return 1;
  return 0;
}
