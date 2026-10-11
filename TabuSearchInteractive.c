#include <float.h>
#include <math.h>
#include <stdlib.h>

#include "TabuSearchInteractive.h"
#include "optimizers.h"

#define PI 3.14159265358979323846
#define DIRECTION_COUNT 8
#define MAX_TESTED_POINTS 100000
#define INITIAL_STEP_FRACTION 0.02
#define MIN_STEP_FRACTION 0.0000001
#define TABU_TENURE 24
#define TABU_RADIUS_FRACTION 0.008
#define MAX_STAGNANT_MOVES 80

#define SCOUT_AGENT_TYPE 5
#define SCOUT_ACTIVE_TYPE 6
#define SCOUT_INACTIVE_TYPE 7
#define SCOUT_TESTED_TYPE 8

typedef struct
{
    double x;
    double y;
} Point;

static const double RelativeAngles[DIRECTION_COUNT] =
{
    0.0, 180.0, 90.0, 270.0, 45.0, 135.0, 225.0, 315.0
};

static const TestProblem *currentProblem;
static Point testedPoints[MAX_TESTED_POINTS];
static int testedPointCount;
static Point tabu[TABU_TENURE];
static int tabuCount;
static int tabuNext;
static int testedDirections[DIRECTION_COUNT];

static double agentX, agentY, agentValue, agentAngle;
static double bestX, bestY, bestValue;
static double stepSize, initialStep, minimumStep, tabuRadiusSquared;
static int evaluations;
static int stagnantMoves;

static double NormalizeAngle(double angle)
{
    angle = fmod(angle, 2.0 * PI);

    if (angle < 0.0)
        angle += 2.0 * PI;

    return angle;
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

static void RememberPosition(double x, double y)
{
    tabu[tabuNext].x = x;
    tabu[tabuNext].y = y;
    tabuNext = (tabuNext + 1) % TABU_TENURE;

    if (tabuCount < TABU_TENURE)
        tabuCount++;
}

static int IsTabu(double x, double y)
{
    for (int i = 0; i < tabuCount; i++)
    {
        double dx = x - tabu[i].x;
        double dy = y - tabu[i].y;

        if (dx * dx + dy * dy < tabuRadiusSquared)
            return 1;
    }

    return 0;
}

static void Restart(void)
{
    agentX = RandomDouble(currentProblem->lower, currentProblem->upper);
    agentY = RandomDouble(currentProblem->lower, currentProblem->upper);
    agentAngle = RandomDouble(0.0, 2.0 * PI);
    stepSize = initialStep;
    stagnantMoves = 0;
    tabuCount = 0;
    tabuNext = 0;
    ResetDirections();
    agentValue = Evaluate(agentX, agentY);
    RememberPosition(agentX, agentY);
}

static void TabuSearchInit(const TestProblem *problem)
{
    currentProblem = problem;
    double range = problem->upper - problem->lower;
    initialStep = range * INITIAL_STEP_FRACTION;
    minimumStep = range * MIN_STEP_FRACTION;
    double radius = range * TABU_RADIUS_FRACTION;
    tabuRadiusSquared = radius * radius;

    evaluations = 0;
    testedPointCount = 0;
    bestValue = DBL_MAX;
    bestX = 0.0;
    bestY = 0.0;
    Restart();
}

static void TabuSearchStep(void)
{
    double candidateX = agentX;
    double candidateY = agentY;
    double candidateValue = DBL_MAX;
    double candidateAngle = agentAngle;
    int found = 0;
    ResetDirections();

    /* Select the best admissible neighbour, even if it is uphill.
       Aspiration: a new global best overrides tabu status. */
    for (int i = 0; i < DIRECTION_COUNT; i++)
    {
        double angle = NormalizeAngle(agentAngle + RelativeAngles[i] * PI / 180.0);
        double x = agentX + stepSize * cos(angle);
        double y = agentY + stepSize * sin(angle);

        if (x < currentProblem->lower || x > currentProblem->upper ||
            y < currentProblem->lower || y > currentProblem->upper)
            continue;

        double previousBest = bestValue;
        double value = Evaluate(x, y);
        testedDirections[i] = 1;

        if (IsTabu(x, y) && value >= previousBest)
            continue;

        if (!found || value < candidateValue)
        {
            found = 1;
            candidateX = x;
            candidateY = y;
            candidateValue = value;
            candidateAngle = angle;
        }
    }

    if (!found)
    {
        stepSize *= 0.5;

        if (stepSize < minimumStep)
            Restart();

        ResetDirections();
        return;
    }

    double previousValue = agentValue;
    agentX = candidateX;
    agentY = candidateY;
    agentValue = candidateValue;
    agentAngle = candidateAngle;
    RememberPosition(agentX, agentY);

    if (agentValue < previousValue)
    {
        stagnantMoves = 0;
        stepSize = fmax(minimumStep, stepSize * 0.85);
    }
    else
    {
        stagnantMoves++;
    }

    if (stagnantMoves >= MAX_STAGNANT_MOVES)
        Restart();

    ResetDirections();
}

static int TabuSearchGetPointCount(void)
{
    return testedPointCount + 1 + DIRECTION_COUNT;
}

static void TabuSearchGetPoint(int index, double *x, double *y, int *type)
{
    if (index < 0 || index >= TabuSearchGetPointCount())
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

static void TabuSearchGetBest(double *x, double *y, double *value, int *count)
{
    *x = bestX;
    *y = bestY;
    *value = bestValue;
    *count = evaluations;
}

InteractiveOptimizer TabuSearchOptimizer =
{
    "TabuSearch",
    TabuSearchInit,
    TabuSearchStep,
    TabuSearchGetPointCount,
    TabuSearchGetPoint,
    TabuSearchGetBest
};
