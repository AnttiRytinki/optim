#include <math.h>

#include "optimizers.h"
#include "problems.h"

double Sphere(const double* x, int dim)
{
    double sum = 0.0;

    for (int i = 0; i < dim; i++)
        sum += x[i] * x[i];

    return sum;
}

double Rastrigin(const double* x, int dim)
{
    double sum = 10.0 * dim;

    for (int i = 0; i < dim; i++)
        sum += x[i] * x[i] - 10.0 * cos(2.0 * M_PI * x[i]);

    return sum;
}

double Rosenbrock(const double* x, int dim)
{
    double sum = 0.0;

    for (int i = 0; i < dim - 1; i++) {
        double a = x[i + 1] - x[i] * x[i];
        double b = 1.0 - x[i];

        sum += 100.0 * a * a + b * b;
    }

    return sum;
}

double Ackley(const double* x, int dim)
{
    double sumSquares = 0.0;
    double sumCosines = 0.0;

    for (int i = 0; i < dim; i++) {
        sumSquares += x[i] * x[i];
        sumCosines += cos(2.0 * M_PI * x[i]);
    }

    double term1 = -20.0 * exp(-0.2 * sqrt(sumSquares / dim));

    double term2 = -exp(sumCosines / dim);

    return term1 + term2 + 20.0 + M_E;
}

double Himmelblau(const double* x, int dim)
{
    (void)dim;

    double a = x[0] * x[0]
        + x[1]
        - 11.0;

    double b = x[0]
        + x[1] * x[1]
        - 7.0;

    return a * a + b * b;
}

TestProblem SphereProblem = {
    .name = "Sphere",
    .dim = 2,
    .lower = -5.12,
    .upper = 5.12,
    .optimum = { 0.0, 0.0 },
    .optimumValue = 0.0,
    .function = Sphere
};

TestProblem RastriginProblem = {
    .name = "Rastrigin",
    .dim = 2,
    .lower = -5.12,
    .upper = 5.12,
    .optimum = { 0.0, 0.0 },
    .optimumValue = 0.0,
    .function = Rastrigin
};

TestProblem RosenbrockProblem = {
    .name = "Rosenbrock",
    .dim = 2,
    .lower = -2.0,
    .upper = 2.0,
    .optimum = { 1.0, 1.0 },
    .optimumValue = 0.0,
    .function = Rosenbrock
};

TestProblem AckleyProblem = {
    .name = "Ackley",
    .dim = 2,
    .lower = -5.0,
    .upper = 5.0,
    .optimum = { 0.0, 0.0 },
    .optimumValue = 0.0,
    .function = Ackley
};

TestProblem HimmelblauProblem = {
    .name = "Himmelblau",
    .dim = 2,
    .lower = -5.0,
    .upper = 5.0,
    .optimum = { 3.0, 2.0 },
    .optimumValue = 0.0,
    .function = Himmelblau
};
