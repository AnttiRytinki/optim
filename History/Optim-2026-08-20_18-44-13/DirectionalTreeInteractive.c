#include <math.h>
#include <stdlib.h>

#include "optimizers.h"

#define AGENT_COUNT 8
#define DIRECTION_COUNT 8
#define MAX_TESTED_POINTS 100000

#define MAX_STEP_FRACTION 0.02
#define HQ_SEARCH_TOLERANCE_FRACTION 0.002
#define HQ_MAX_SEARCH_NODES 20000

#define DIRECTIONAL_TREE_AGENT_TYPE 5
#define DIRECTIONAL_TREE_ACTIVE_TYPE 6
#define DIRECTIONAL_TREE_INACTIVE_TYPE 7
#define DIRECTIONAL_TREE_TESTED_TYPE 8

#define PHASE_FIRST 0
#define PHASE_OPPOSITE 1
#define PHASE_PERPENDICULAR_1 2
#define PHASE_PERPENDICULAR_2 3

typedef struct
{
    double position[MAX_DIM];
    double value;
    int tested;
} LocalPoint;

typedef struct
{
    double position[MAX_DIM];
    double currentValue;
    double stepSize;

    int baseDirection;
    int rotated;
    int phase;

    LocalPoint localPoints[DIRECTION_COUNT];
} Agent;

typedef struct
{
    double position[MAX_DIM];
    double value;
} TestedPoint;

typedef struct
{
    double cx;
    double cy;
    double halfSize;
    double clearance;
    double upperBound;
} SearchRegion;

static const TestProblem* activeProblem;
static AlgorithmResult result;

static Agent agents[AGENT_COUNT];

static TestedPoint testedPoints[MAX_TESTED_POINTS];
static int testedPointCount;

static const int DirectionX[DIRECTION_COUNT] = {
    0,
    1,
    1,
    1,
    0,
    -1,
    -1,
    -1
};

static const int DirectionY[DIRECTION_COUNT] = {
    -1,
    -1,
    0,
    1,
    1,
    1,
    0,
    -1
};

static void CopyVector(double* dst, const double* src, int dim)
{
    for (int i = 0; i < dim; i++)
        dst[i] = src[i];
}

static void Clamp(double* x, const TestProblem* problem)
{
    for (int i = 0; i < problem->dim; i++) {
        if (x[i] < problem->lower)
            x[i] = problem->lower;

        if (x[i] > problem->upper)
            x[i] = problem->upper;
    }
}

static void RegisterTestedPoint(const double* position, double value)
{
    if (testedPointCount >= MAX_TESTED_POINTS)
        return;

    CopyVector(
        testedPoints[testedPointCount].position,
        position,
        activeProblem->dim);

    testedPoints[testedPointCount].value = value;

    testedPointCount++;
}

static double Evaluate(const double* position)
{
    double value = activeProblem->function(
        position,
        activeProblem->dim);

    result.evaluations++;

    RegisterTestedPoint(position, value);

    if (value < result.bestValue) {
        result.bestValue = value;

        CopyVector(
            result.bestX,
            position,
            activeProblem->dim);
    }

    return value;
}

static void BuildLocalPoint(
    Agent* agent,
    int direction)
{
    LocalPoint* point = &agent->localPoints[direction];

    CopyVector(
        point->position,
        agent->position,
        activeProblem->dim);

    point->position[0] += DirectionX[direction]
        * agent->stepSize;

    point->position[1] += DirectionY[direction]
        * agent->stepSize;

    Clamp(
        point->position,
        activeProblem);

    point->tested = 0;
    point->value = INFINITY;
}

static void ResetLocalPoints(Agent* agent)
{
    for (int i = 0; i < DIRECTION_COUNT; i++)
        BuildLocalPoint(agent, i);
}

static void StartLocalSearch(Agent* agent)
{
    agent->baseDirection = rand() % DIRECTION_COUNT;

    agent->rotated = 0;
    agent->phase = PHASE_FIRST;

    ResetLocalPoints(agent);
}

static void MoveAgent(
    Agent* agent,
    int direction)
{
    LocalPoint* point = &agent->localPoints[direction];

    CopyVector(
        agent->position,
        point->position,
        activeProblem->dim);

    agent->currentValue = point->value;

    StartLocalSearch(agent);
}

static void TestDirection(
    Agent* agent,
    int direction)
{
    LocalPoint* point = &agent->localPoints[direction];

    point->value = Evaluate(point->position);

    point->tested = 1;
}

static int GetDirection(
    const Agent* agent,
    int offset)
{
    return (agent->baseDirection + offset)
        % DIRECTION_COUNT;
}

static int FindBestTestedDirection(
    const Agent* agent)
{
    int bestDirection = -1;
    double bestValue = agent->currentValue;

    for (int i = 0; i < DIRECTION_COUNT; i++) {
        if (!agent->localPoints[i].tested)
            continue;

        if (agent->localPoints[i].value < bestValue) {
            bestValue = agent->localPoints[i].value;

            bestDirection = i;
        }
    }

    return bestDirection;
}

static void RotateLocalSearch(Agent* agent)
{
    agent->baseDirection = (agent->baseDirection + 1)
        % DIRECTION_COUNT;

    agent->rotated = 1;
    agent->phase = PHASE_FIRST;
}

static double GetClearance(double x, double y)
{
    double clearance = fmin(
        fmin(
            x - activeProblem->lower,
            activeProblem->upper - x),
        fmin(
            y - activeProblem->lower,
            activeProblem->upper - y));

    for (int i = 0; i < testedPointCount; i++) {
        double dx = fabs(
            x
            - testedPoints[i].position[0]);

        double dy = fabs(
            y
            - testedPoints[i].position[1]);

        double distance = fmax(dx, dy);

        if (distance < clearance)
            clearance = distance;
    }

    return clearance;
}

static void FindLargestEmptySquare(
    double* bestX,
    double* bestY)
{
    SearchRegion regions[HQ_MAX_SEARCH_NODES];

    int regionCount = 1;

    double width = activeProblem->upper
        - activeProblem->lower;

    regions[0].cx = (activeProblem->lower
                        + activeProblem->upper)
        * 0.5;

    regions[0].cy = regions[0].cx;

    regions[0].halfSize = width * 0.5;

    regions[0].clearance = GetClearance(
        regions[0].cx,
        regions[0].cy);

    regions[0].upperBound = regions[0].clearance
        + regions[0].halfSize;

    double bestClearance = regions[0].clearance;

    *bestX = regions[0].cx;
    *bestY = regions[0].cy;

    double tolerance = width
        * HQ_SEARCH_TOLERANCE_FRACTION;

    int processed = 0;

    while (
        regionCount > 0
        && processed < HQ_MAX_SEARCH_NODES) {
        int bestRegion = 0;

        for (int i = 1; i < regionCount; i++) {
            if (
                regions[i].upperBound
                > regions[bestRegion].upperBound) {
                bestRegion = i;
            }
        }

        SearchRegion region = regions[bestRegion];

        regions[bestRegion] = regions[regionCount - 1];

        regionCount--;
        processed++;

        if (
            region.upperBound
            <= bestClearance) {
            continue;
        }

        if (
            region.halfSize
            <= tolerance) {
            continue;
        }

        double childHalf = region.halfSize * 0.5;

        static const int offsets[4][2] = {
            { -1, -1 },
            { 1, -1 },
            { -1, 1 },
            { 1, 1 }
        };

        for (int c = 0; c < 4; c++) {
            if (
                regionCount
                >= HQ_MAX_SEARCH_NODES) {
                break;
            }

            SearchRegion child;

            child.cx = region.cx
                + offsets[c][0]
                    * childHalf;

            child.cy = region.cy
                + offsets[c][1]
                    * childHalf;

            child.halfSize = childHalf;

            child.clearance = GetClearance(
                child.cx,
                child.cy);

            child.upperBound = child.clearance
                + child.halfSize;

            if (
                child.clearance
                > bestClearance) {
                bestClearance = child.clearance;

                *bestX = child.cx;
                *bestY = child.cy;
            }

            if (
                child.upperBound
                > bestClearance) {
                regions[regionCount] = child;

                regionCount++;
            }
        }
    }
}

static void TeleportAgentToLargestHole(
    Agent* agent)
{
    double x;
    double y;

    FindLargestEmptySquare(
        &x,
        &y);

    agent->position[0] = x;
    agent->position[1] = y;

    for (int d = 2; d < activeProblem->dim; d++) {
        agent->position[d] = RandomDouble(
            activeProblem->lower,
            activeProblem->upper);
    }

    agent->currentValue = Evaluate(agent->position);

    agent->stepSize = (activeProblem->upper
                          - activeProblem->lower)
        * MAX_STEP_FRACTION;

    StartLocalSearch(agent);
}

static void StepAgent(Agent* agent)
{
    int first = GetDirection(agent, 0);

    int opposite = GetDirection(agent, 4);

    int perpendicular1 = GetDirection(agent, 2);

    int perpendicular2 = GetDirection(agent, 6);

    if (agent->phase == PHASE_FIRST) {
        TestDirection(
            agent,
            first);

        if (
            agent->localPoints[first].value
            < agent->currentValue) {
            MoveAgent(
                agent,
                first);

            return;
        }

        agent->phase = PHASE_OPPOSITE;

        return;
    }

    if (agent->phase == PHASE_OPPOSITE) {
        TestDirection(
            agent,
            opposite);

        if (
            agent->localPoints[opposite].value
            < agent->currentValue) {
            MoveAgent(
                agent,
                opposite);

            return;
        }

        agent->phase = PHASE_PERPENDICULAR_1;

        return;
    }

    if (
        agent->phase
        == PHASE_PERPENDICULAR_1) {
        TestDirection(
            agent,
            perpendicular1);

        agent->phase = PHASE_PERPENDICULAR_2;

        return;
    }

    if (
        agent->phase
        == PHASE_PERPENDICULAR_2) {
        TestDirection(
            agent,
            perpendicular2);

        int bestDirection = FindBestTestedDirection(agent);

        if (bestDirection >= 0) {
            MoveAgent(
                agent,
                bestDirection);

            return;
        }

        if (!agent->rotated) {
            RotateLocalSearch(agent);
            return;
        }

        /*
         * Centre is lower than all eight
         * neighbouring grid positions.
         *
         * The agent reports that it is stuck.
         * HQ finds the largest unexplored hole
         * and teleports the agent there.
         */
        TeleportAgentToLargestHole(agent);
    }
}

static void DirectionalTreeInteractiveInit(
    const TestProblem* problem)
{
    activeProblem = problem;

    result.evaluations = 0;
    result.bestValue = INFINITY;

    testedPointCount = 0;

    double maximumStep = (problem->upper
                             - problem->lower)
        * MAX_STEP_FRACTION;

    for (int a = 0; a < AGENT_COUNT; a++) {
        Agent* agent = &agents[a];

        for (int d = 0; d < problem->dim; d++) {
            agent->position[d] = RandomDouble(
                problem->lower,
                problem->upper);
        }

        agent->stepSize = maximumStep;

        agent->currentValue = Evaluate(agent->position);

        StartLocalSearch(agent);
    }
}

static void DirectionalTreeInteractiveStep(void)
{
    if (activeProblem == NULL)
        return;

    for (int a = 0; a < AGENT_COUNT; a++)
        StepAgent(&agents[a]);
}

static int DirectionalTreeInteractiveGetPointCount(void)
{
    return testedPointCount
        + AGENT_COUNT
        + AGENT_COUNT * DIRECTION_COUNT;
}

static void DirectionalTreeInteractiveGetPoint(
    int index,
    double* x,
    double* y,
    int* type)
{
    if (index < testedPointCount) {
        *x = testedPoints[index].position[0];

        *y = testedPoints[index].position[1];

        *type = DIRECTIONAL_TREE_TESTED_TYPE;

        return;
    }

    index -= testedPointCount;

    if (index < AGENT_COUNT) {
        *x = agents[index].position[0];

        *y = agents[index].position[1];

        *type = DIRECTIONAL_TREE_AGENT_TYPE;

        return;
    }

    index -= AGENT_COUNT;

    int agentIndex = index / DIRECTION_COUNT;

    int direction = index % DIRECTION_COUNT;

    LocalPoint* point = &agents[agentIndex].localPoints[direction];

    *x = point->position[0];

    *y = point->position[1];

    if (point->tested) {
        *type = DIRECTIONAL_TREE_INACTIVE_TYPE;
    } else {
        *type = DIRECTIONAL_TREE_ACTIVE_TYPE;
    }
}

static void DirectionalTreeInteractiveGetBest(
    double* x,
    double* y,
    double* value,
    int* evaluations)
{
    *x = result.bestX[0];

    *y = result.bestX[1];

    *value = result.bestValue;

    *evaluations = result.evaluations;
}

InteractiveOptimizer DirectionalTreeOptimizer = {
    "DirectionalTree",
    DirectionalTreeInteractiveInit,
    DirectionalTreeInteractiveStep,
    DirectionalTreeInteractiveGetPointCount,
    DirectionalTreeInteractiveGetPoint,
    DirectionalTreeInteractiveGetBest
};
