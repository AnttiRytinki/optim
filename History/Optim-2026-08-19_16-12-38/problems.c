#include <math.h>

#include "optimizers.h"
#include "problems.h"

double Sphere(const double *x, int dim)
{
    double sum = 0.0;

    for (int i = 0; i < dim; i++)
        sum += x[i] * x[i];

    return sum;
}

double Rastrigin(const double *x, int dim)
{
    double sum = 10.0 * dim;

    for (int i = 0; i < dim; i++)
        sum += x[i] * x[i] - 10.0 * cos(2.0 * M_PI * x[i]);

    return sum;
}

double Rosenbrock(const double *x, int dim)
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

TestProblem SphereProblem =
{
    .name = "Sphere",
    .dim = 2,
    .lower = -5.12,
    .upper = 5.12,
    .optimum = { 0.0, 0.0 },
    .optimumValue = 0.0,
    .function = Sphere
};

TestProblem RastriginProblem =
{
    .name = "Rastrigin",
    .dim = 2,
    .lower = -5.12,
    .upper = 5.12,
    .optimum = { 0.0, 0.0 },
    .optimumValue = 0.0,
    .function = Rastrigin
};

TestProblem RosenbrockProblem =
{
    .name = "Rosenbrock",
    .dim = 2,
    .lower = -2.0,
    .upper = 2.0,
    .optimum = { 1.0, 1.0 },
    .optimumValue = 0.0,
    .function = Rosenbrock
};
