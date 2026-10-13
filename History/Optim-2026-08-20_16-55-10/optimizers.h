#ifndef OPTIMIZERS_H
#define OPTIMIZERS_H

#define MAX_DIM 16

typedef double (*ObjectiveFunction)(const double *x, int dim);

typedef struct
{
    const char *name;
    int dim;
    double lower;
    double upper;
    double optimum[MAX_DIM];
    double optimumValue;
    ObjectiveFunction function;
} TestProblem;

typedef struct
{
    double bestX[MAX_DIM];
    double bestValue;
    int evaluations;
} AlgorithmResult;

typedef struct
{
    const char *name;

    void (*Init)(const TestProblem *problem);
    void (*Step)(void);

    int (*GetPointCount)(void);
    void (*GetPoint)(int index, double *x, double *y, int *type);

    void (*GetBest)(double *x, double *y, double *value, int *evaluations);
} InteractiveOptimizer;

extern InteractiveOptimizer *Optimizers[];
extern int OptimizerCount;

double RandomDouble(double min, double max);

#endif
