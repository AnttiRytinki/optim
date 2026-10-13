#ifndef INTERACTIVE_OPTIMIZER_H
#define INTERACTIVE_OPTIMIZER_H

#include "optimizers.h"

typedef struct
{
    const char *name;

    void (*Init)(const TestProblem *problem);
    void (*Step)(void);

    int (*GetPointCount)(void);
    void (*GetPoint)(int index, double *x, double *y, int *type);

    void (*GetBest)(double *x, double *y, double *value, int *evaluations);
} InteractiveOptimizer;

#endif
