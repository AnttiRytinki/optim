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

double RandomDouble(double min, double max);

AlgorithmResult RandomSearch(const TestProblem *problem, int maxEvaluations);
AlgorithmResult HillClimb(const TestProblem *problem, int maxEvaluations);
AlgorithmResult DifferentialEvolution(const TestProblem *problem, int maxEvaluations);
AlgorithmResult ExploreExploitSwarm(const TestProblem *problem, int maxEvaluations);

#endif
