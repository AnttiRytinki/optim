#ifndef NOMAD_ADAPTER_H
#define NOMAD_ADAPTER_H

#include "optimizers.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct NomadRun NomadRun;

typedef struct
{
    double point[MAX_DIM];
    double value;
    double bestPoint[MAX_DIM];
    double bestValue;
    double elapsedMs;
} NomadEvaluation;

/* Serial, synchronous MADS. Not thread-safe: NOMAD has process-wide state.
 * The start is supplied by the caller; its evaluation is part of the budget.
 * Returns NULL on failure and writes an error into the caller's buffer.
 * No objective knowledge, optimum value, or success threshold is used by MADS.
 */
NomadRun *NomadOptimize(const TestProblem *problem, const double *start,
                       int seed, int budget, char *error, size_t errorSize);
void NomadDestroy(NomadRun *run);
size_t NomadEvaluationCount(const NomadRun *run);
size_t NomadReportedCount(const NomadRun *run);
double NomadElapsedMs(const NomadRun *run);
const NomadEvaluation *NomadEvaluationAt(const NomadRun *run, size_t index);
/* One-based budget; carries final best forward without inventing evaluations. */
const NomadEvaluation *NomadAtBudget(const NomadRun *run, size_t budget);
/* Last completed black-box evaluation by this time, or NULL if none.
 * Times include reset, parameter setup, models, and optimizer initialization.
 */
const NomadEvaluation *NomadAtTime(const NomadRun *run, double elapsedMs);

#ifdef __cplusplus
}
#endif
#endif
