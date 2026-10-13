#include <float.h>
#include <math.h>
#include <stdlib.h>

#include "BasinHoppingInteractive.h"
#include "optimizers.h"

#define PI 3.14159265358979323846
#define DIRECTION_COUNT 8
#define MAX_TESTED_POINTS 100000
#define INITIAL_STEP_FRACTION 0.02
#define MIN_STEP_FRACTION 0.0000001
#define HOP_FRACTION 0.10
#define METROPOLIS_TEMPERATURE 1.0

#define SCOUT_AGENT_TYPE 5
#define SCOUT_ACTIVE_TYPE 6
#define SCOUT_INACTIVE_TYPE 7
#define SCOUT_TESTED_TYPE 8

static const double RelativeAngles[DIRECTION_COUNT] =
{
    0.0, 180.0, 90.0, 270.0, 45.0, 135.0, 225.0, 315.0
};

typedef struct
{
    double x;
    double y;
} Point;

static const TestProblem *currentProblem;
static Point testedPoints[MAX_TESTED_POINTS];
static int testedPointCount;
static int testedDirections[DIRECTION_COUNT];

static double agentX, agentY, agentValue, agentAngle;
static double basinX, basinY, basinValue;
static double bestX, bestY, bestValue;
static double initialStep, minimumStep, stepSize;
static int evaluations;

static double NormalizeAngle(double angle)
{
    angle = fmod(angle, 2.0 * PI);

    if (angle < 0.0)
        angle += 2.0 * PI;

    return angle;
}

static double Clamp(double value)
{
    if (value < currentProblem->lower)
        return currentProblem->lower;

    if (value > currentProblem->upper)
        return currentProblem->upper;

    return value;
}

static void ResetDirections(void)
{
    for (int i = 0; i < DIRECTION_COUNT; i++)
        testedDirections[i] = 0;
}

static double Evaluate(double x, double y)
{
    double position[MAX_DIM] = {0.0};

    position[0] = x;
    position[1] = y;

    for (int d = 2; d < currentProblem->dim; d++)
        position[d] = RandomDouble(currentProblem->lower, currentProblem->upper);

    double value = currentProblem->function(position, currentProblem->dim);
    evaluations++;

    if (testedPointCount < MAX_TESTED_POINTS)
    {
        testedPoints[testedPointCount].x = x;
        testedPoints[testedPointCount].y = y;
        testedPointCount++;
    }

    if (value < bestValue)
    {
        bestValue = value;
        bestX = x;
        bestY = y;
    }

    return value;
}

static void StartLocalSearch(double x, double y)
{
    agentX = x;
    agentY = y;
    agentAngle = RandomDouble(0.0, 2.0 * PI);
    stepSize = initialStep;
    ResetDirections();
    agentValue = Evaluate(x, y);
}

static void BasinHoppingInit(const TestProblem *problem)
{
    currentProblem = problem;
    initialStep = (problem->upper - problem->lower) * INITIAL_STEP_FRACTION;
    minimumStep = (problem->upper - problem->lower) * MIN_STEP_FRACTION;

    evaluations = 0;
    testedPointCount = 0;
    bestValue = DBL_MAX;
    bestX = 0.0;
    bestY = 0.0;

    double x = RandomDouble(problem->lower, problem->upper);
    double y = RandomDouble(problem->lower, problem->upper);

    StartLocalSearch(x, y);
    basinX = x;
    basinY = y;
    basinValue = DBL_MAX;
}

static int TestDirection(int direction, double *candidateX, double *candidateY,
                         double *candidateValue, double *candidateAngle)
{
    double angle = NormalizeAngle(agentAngle + RelativeAngles[direction] * PI / 180.0);
    double x = agentX + stepSize * cos(angle);
    double y = agentY + stepSize * sin(angle);

    if (x < currentProblem->lower || x > currentProblem->upper ||
        y < currentProblem->lower || y > currentProblem->upper)
        return 0;

    double value = Evaluate(x, y);
    testedDirections[direction] = 1;

    if (value >= *candidateValue)
        return 0;

    *candidateX = x;
    *candidateY = y;
    *candidateValue = value;
    *candidateAngle = angle;
    return 1;
}

static void FinishLocalSearch(void)
{
    /* Metropolis acceptance compares fully minimized basins, not hop starts. */
    int accept = basinValue == DBL_MAX || agentValue <= basinValue;

    if (!accept)
    {
        double probability = exp((basinValue - agentValue) / METROPOLIS_TEMPERATURE);
        accept = RandomDouble(0.0, 1.0) < probability;
    }

    if (accept)
    {
        basinX = agentX;
        basinY = agentY;
        basinValue = agentValue;
    }

    /* Always propose from the last accepted basin, even after rejection. */
    double hop = (currentProblem->upper - currentProblem->lower) * HOP_FRACTION;
    double x = Clamp(basinX + RandomDouble(-hop, hop));
    double y = Clamp(basinY + RandomDouble(-hop, hop));
    StartLocalSearch(x, y);
}

static void BasinHoppingStep(void)
{
    double candidateX = agentX;
    double candidateY = agentY;
    double candidateValue = agentValue;
    double candidateAngle = agentAngle;

    ResetDirections();

    if (TestDirection(0, &candidateX, &candidateY, &candidateValue, &candidateAngle))
        goto MoveAgent;

    if (TestDirection(1, &candidateX, &candidateY, &candidateValue, &candidateAngle))
        goto MoveAgent;

    for (int i = 2; i < 4; i++)
        TestDirection(i, &candidateX, &candidateY, &candidateValue, &candidateAngle);

    if (candidateValue < agentValue)
        goto MoveAgent;

    for (int i = 4; i < DIRECTION_COUNT; i++)
        TestDirection(i, &candidateX, &candidateY, &candidateValue, &candidateAngle);

    if (candidateValue < agentValue)
        goto MoveAgent;

    stepSize *= 0.5;

    if (stepSize < minimumStep)
        FinishLocalSearch();

    ResetDirections();
    return;

MoveAgent:
    agentX = candidateX;
    agentY = candidateY;
    agentValue = candidateValue;
    agentAngle = candidateAngle;
    ResetDirections();
}

static int BasinHoppingGetPointCount(void)
{
    return testedPointCount + 1 + DIRECTION_COUNT;
}

static void BasinHoppingGetPoint(int index, double *x, double *y, int *type)
{
    if (index < 0 || index >= BasinHoppingGetPointCount())
        return;

    if (index < testedPointCount)
    {
        *x = testedPoints[index].x;
        *y = testedPoints[index].y;
        *type = SCOUT_TESTED_TYPE;
        return;
    }

    index -= testedPointCount;

    if (index == 0)
    {
        *x = agentX;
        *y = agentY;
        *type = SCOUT_AGENT_TYPE;
        return;
    }

    int direction = index - 1;
    double angle = NormalizeAngle(agentAngle + RelativeAngles[direction] * PI / 180.0);
    *x = agentX + stepSize * cos(angle);
    *y = agentY + stepSize * sin(angle);
    *type = testedDirections[direction] ? SCOUT_INACTIVE_TYPE : SCOUT_ACTIVE_TYPE;
}

static void BasinHoppingGetBest(double *x, double *y, double *value, int *count)
{
    *x = bestX;
    *y = bestY;
    *value = bestValue;
    *count = evaluations;
}

InteractiveOptimizer BasinHoppingOptimizer =
{
    "BasinHopping",
    BasinHoppingInit,
    BasinHoppingStep,
    BasinHoppingGetPointCount,
    BasinHoppingGetPoint,
    BasinHoppingGetBest
};
