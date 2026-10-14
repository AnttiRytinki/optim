#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "EchoInteractive.h"
#include "optimizers.h"

/* Echo v1: one agent, stored samples, local quadratic surrogate,
   trust-radius adaptation, and coverage-guided exploration. */
#define ECHO_MEMORY_CAP 100000
#define ECHO_GRID 32
#define ECHO_NEIGHBORS 24
#define ECHO_FEATURES 6
#define ECHO_CANDIDATES 96
#define ECHO_AGENT_TYPE 5
#define ECHO_TESTED_TYPE 8

/* Position and value are kept together to make the memory reusable. */
typedef struct
{
    double x, y, value;
} EchoSample;

static const TestProblem *problem;
static EchoSample samples[ECHO_MEMORY_CAP];
static int sampleCount;
static unsigned int visits[ECHO_GRID][ECHO_GRID];
static double gridBest[ECHO_GRID][ECHO_GRID];
static double agentX, agentY, agentValue;
static double bestX, bestY, bestValue;
static double radius, minRadius, maxRadius;
static int evaluations, failures;
static int exploring;
static double prediction;
static double proposalX, proposalY;

static double Clamp(double x, double low, double high)
{
    if (x < low) return low;
    if (x > high) return high;
    return x;
}

static double Range(void)
{
    return problem->upper - problem->lower;
}

static double Distance2(double x, double y, double a, double b)
{
    double dx = x - a, dy = y - b;
    return dx * dx + dy * dy;
}

static int Cell(double x)
{
    int index = (int)((x - problem->lower) / Range() * ECHO_GRID);
    if (index < 0) index = 0;
    if (index >= ECHO_GRID) index = ECHO_GRID - 1;
    return index;
}

static double Evaluate(double x, double y)
{
    double position[MAX_DIM] = {0.0};
    position[0] = x;
    position[1] = y;
    for (int d = 2; d < problem->dim; ++d)
        position[d] = RandomDouble(problem->lower, problem->upper);

    double value = problem->function(position, problem->dim);
    ++evaluations;
    int cellX = Cell(x), cellY = Cell(y);
    ++visits[cellX][cellY];
    if (value < gridBest[cellX][cellY]) gridBest[cellX][cellY] = value;
    if (sampleCount < ECHO_MEMORY_CAP)
    {
        samples[sampleCount].x = x;
        samples[sampleCount].y = y;
        samples[sampleCount].value = value;
        ++sampleCount;
    }
    if (value < bestValue)
    {
        bestValue = value;
        bestX = x;
        bestY = y;
    }
    return value;
}

/* Solve a six-variable least-squares system with partial pivoting. */
static int Solve(double a[ECHO_FEATURES][ECHO_FEATURES + 1], double result[ECHO_FEATURES])
{
    for (int col = 0; col < ECHO_FEATURES; ++col)
    {
        int pivot = col;
        for (int row = col + 1; row < ECHO_FEATURES; ++row)
            if (fabs(a[row][col]) > fabs(a[pivot][col])) pivot = row;
        if (fabs(a[pivot][col]) < 1e-13) return 0;
        if (pivot != col)
            for (int j = col; j <= ECHO_FEATURES; ++j)
            {
                double t = a[col][j];
                a[col][j] = a[pivot][j];
                a[pivot][j] = t;
            }
        double divisor = a[col][col];
        for (int j = col; j <= ECHO_FEATURES; ++j) a[col][j] /= divisor;
        for (int row = 0; row < ECHO_FEATURES; ++row)
        {
            if (row == col) continue;
            double factor = a[row][col];
            for (int j = col; j <= ECHO_FEATURES; ++j)
                a[row][j] -= factor * a[col][j];
        }
    }
    for (int i = 0; i < ECHO_FEATURES; ++i) result[i] = a[i][ECHO_FEATURES];
    return 1;
}

/* Fit around the current agent, using the closest remembered measurements.
   Coordinates are normalized by the trust radius for conditioning. */
static int FitModel(double coefficients[ECHO_FEATURES])
{
    if (sampleCount < ECHO_FEATURES + 2) return 0;
    int nearest[ECHO_NEIGHBORS];
    double distances[ECHO_NEIGHBORS];
    int found = 0;
    /* Bounded search cost: uniformly sampled history plus recent points. */
    int stride = sampleCount > 512 ? sampleCount / 512 : 1;
    for (int i = 0; i < sampleCount; i += stride)
    {
        double distance = Distance2(agentX, agentY, samples[i].x, samples[i].y);
        int at = found;
        if (at > ECHO_NEIGHBORS - 1) at = ECHO_NEIGHBORS - 1;
        while (at > 0 && distance < distances[at - 1]) --at;
        if (at >= ECHO_NEIGHBORS) continue;
        if (found < ECHO_NEIGHBORS) ++found;
        for (int j = found - 1; j > at; --j)
        {
            nearest[j] = nearest[j - 1];
            distances[j] = distances[j - 1];
        }
        nearest[at] = i;
        distances[at] = distance;
    }
    if (found < ECHO_FEATURES + 2) return 0;

    double scale = fmax(radius, Range() * 1e-7);
    double matrix[ECHO_FEATURES][ECHO_FEATURES + 1] = {{0.0}};
    for (int i = 0; i < found; ++i)
    {
        const EchoSample *s = &samples[nearest[i]];
        double x = (s->x - agentX) / scale;
        double y = (s->y - agentY) / scale;
        double terms[ECHO_FEATURES] = {1.0, x, y, x * x, x * y, y * y};
        double weight = 1.0 / (1.0 + x * x + y * y);
        for (int r = 0; r < ECHO_FEATURES; ++r)
        {
            for (int c = 0; c < ECHO_FEATURES; ++c)
                matrix[r][c] += weight * terms[r] * terms[c];
            matrix[r][ECHO_FEATURES] += weight * terms[r] * (s->value - agentValue);
        }
    }
    /* Small regularization helps when nearby samples are nearly collinear. */
    for (int i = 1; i < ECHO_FEATURES; ++i)
        matrix[i][i] += 1e-9;
    return Solve(matrix, coefficients);
}

static double Predict(const double coefficients[ECHO_FEATURES], double dx, double dy)
{
    return agentValue + coefficients[0] + coefficients[1] * dx +
           coefficients[2] * dy + coefficients[3] * dx * dx +
           coefficients[4] * dx * dy + coefficients[5] * dy * dy;
}

static void Explore(void)
{
    /* Select among least-visited cells; prefer those near good remembered
       values, while keeping a random tie-breaker for global coverage. */
    int chosenX = 0, chosenY = 0;
    double bestScore = DBL_MAX;
    double valueScale = fmax(1.0, fabs(bestValue));
    for (int trial = 0; trial < 96; ++trial)
    {
        int x = rand() % ECHO_GRID, y = rand() % ECHO_GRID;
        double score = (double)visits[x][y] + RandomDouble(0.0, 0.5);
        if (gridBest[x][y] != DBL_MAX)
        {
            double gap = fmax(0.0, gridBest[x][y] - bestValue);
            score += 0.2 * fmin(10.0, gap / valueScale);
        }
        if (score < bestScore)
        {
            bestScore = score;
            chosenX = x;
            chosenY = y;
        }
    }
    double width = Range() / ECHO_GRID;
    proposalX = problem->lower + (chosenX + RandomDouble(0.05, 0.95)) * width;
    proposalY = problem->lower + (chosenY + RandomDouble(0.05, 0.95)) * width;
    exploring = 1;
}

static void EchoInit(const TestProblem *newProblem)
{
    problem = newProblem;
    sampleCount = 0;
    evaluations = 0;
    failures = 0;
    memset(visits, 0, sizeof(visits));
    for (int i = 0; i < ECHO_GRID; ++i)
        for (int j = 0; j < ECHO_GRID; ++j) gridBest[i][j] = DBL_MAX;
    bestValue = DBL_MAX;
    radius = Range() * 0.05;
    maxRadius = Range() * 0.25;
    minRadius = Range() * 1e-7;
    agentX = RandomDouble(problem->lower, problem->upper);
    agentY = RandomDouble(problem->lower, problem->upper);
    agentValue = Evaluate(agentX, agentY);
    exploring = 0;
}

static void EchoStep(void)
{
    double coefficients[ECHO_FEATURES];
    int valid = FitModel(coefficients);
    exploring = 0;
    prediction = agentValue;

    if (failures >= 12 || radius <= minRadius * 1.01)
    {
        Explore();
    }
    else if (valid)
    {
        double chosen = DBL_MAX;
        double scale = fmax(radius, minRadius);
        proposalX = agentX;
        proposalY = agentY;
        /* Search the fitted quadratic model without evaluating the real
           objective; candidate count does not consume evaluation budget. */
        for (int i = 0; i < ECHO_CANDIDATES; ++i)
        {
            double angle = (2.0 * 3.14159265358979323846 * i) / ECHO_CANDIDATES;
            double length = (i % 4 == 0) ? 0.25 : (i % 4 == 1) ? 0.5 :
                            (i % 4 == 2) ? 0.75 : 1.0;
            double x = Clamp(agentX + radius * length * cos(angle), problem->lower, problem->upper);
            double y = Clamp(agentY + radius * length * sin(angle), problem->lower, problem->upper);
            double estimate = Predict(coefficients, (x - agentX) / scale, (y - agentY) / scale);
            if (estimate < chosen && Distance2(x, y, agentX, agentY) > minRadius * minRadius)
            {
                chosen = estimate;
                proposalX = x;
                proposalY = y;
            }
        }
        prediction = chosen;
        if (chosen >= agentValue || !isfinite(chosen))
        {
            radius *= 0.5;
            ++failures;
            /* Still make one real evaluation per Step, to keep the
               benchmark/visualization progressing consistently. */
            double angle = RandomDouble(0.0, 2.0 * 3.14159265358979323846);
            proposalX = Clamp(agentX + radius * cos(angle), problem->lower, problem->upper);
            proposalY = Clamp(agentY + radius * sin(angle), problem->lower, problem->upper);
            prediction = agentValue;
        }
    }
    else
    {
        double angle = RandomDouble(0.0, 2.0 * 3.14159265358979323846);
        proposalX = Clamp(agentX + radius * cos(angle), problem->lower, problem->upper);
        proposalY = Clamp(agentY + radius * sin(angle), problem->lower, problem->upper);
    }

    double oldValue = agentValue;
    double observed = Evaluate(proposalX, proposalY);
    if (exploring)
    {
        agentX = proposalX;
        agentY = proposalY;
        agentValue = observed;
        radius = Range() * 0.05;
        failures = 0;
        return;
    }
    if (observed < oldValue)
    {
        double predictedImprovement = oldValue - prediction;
        double actualImprovement = oldValue - observed;
        if (valid && predictedImprovement > 1e-14)
        {
            double agreement = actualImprovement / predictedImprovement;
            if (agreement > 0.7) radius = fmin(maxRadius, radius * 1.3);
            else if (agreement < 0.2) radius = fmax(minRadius, radius * 0.7);
        }
        agentX = proposalX;
        agentY = proposalY;
        agentValue = observed;
        failures = 0;
    }
    else
    {
        radius = fmax(minRadius, radius * 0.5);
        ++failures;
    }
}

static int EchoGetPointCount(void)
{
    return sampleCount + 1;
}

static void EchoGetPoint(int index, double *x, double *y, int *type)
{
    if (index < 0 || index >= EchoGetPointCount()) return;
    if (index == sampleCount)
    {
        *x = agentX;
        *y = agentY;
        *type = ECHO_AGENT_TYPE;
    }
    else
    {
        *x = samples[index].x;
        *y = samples[index].y;
        *type = ECHO_TESTED_TYPE;
    }
}

static void EchoGetBest(double *x, double *y, double *value, int *count)
{
    *x = bestX;
    *y = bestY;
    *value = bestValue;
    *count = evaluations;
}

InteractiveOptimizer EchoOptimizer =
{
    "Echo",
    EchoInit,
    EchoStep,
    EchoGetPointCount,
    EchoGetPoint,
    EchoGetBest
};
