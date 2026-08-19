#ifndef PROBLEMS_H
#define PROBLEMS_H

#include "optimizers.h"

double Sphere(const double *x, int dim);
double Rastrigin(const double *x, int dim);
double Rosenbrock(const double *x, int dim);

extern TestProblem SphereProblem;
extern TestProblem RastriginProblem;
extern TestProblem RosenbrockProblem;

#endif
