#include <math.h>
#include <stdlib.h>

#include "optimizers.h"

#define AGENT_COUNT 8
#define DIRECTION_COUNT 8
#define MAX_TESTED_POINTS 100000

#define LOCAL_STEP_FRACTION 0.01

#define HQ_GRID_SIZE 20
#define HQ_INTERVAL 20
#define HQ_TELEPORT_COUNT 2

#define HQ_EXPLORATION_WEIGHT 0.5
#define HQ_QUALITY_WEIGHT 0.5

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

static const TestProblem* activeProblem;
static AlgorithmResult result;

static Agent agents[AGENT_COUNT];

static TestedPoint testedPoints[MAX_TESTED_POINTS];
static int testedPointCount;

static int hqStepCounter;
static double localStepSize;

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

    point->position[0] += DirectionX[direction] * localStepSize;

    point->position[1] += DirectionY[direction] * localStepSize;

    Clamp(point->position, activeProblem);

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

static void TeleportAgentRandomly(Agent* agent)
{
    for (int d = 0; d < activeProblem->dim; d++) {
        agent->position[d] = RandomDouble(
            activeProblem->lower,
            activeProblem->upper);
    }

    agent->currentValue = Evaluate(agent->position);

    StartLocalSearch(agent);
}

static void TeleportAgentToCell(
    Agent* agent,
    int cellX,
    int cellY)
{
    double width = activeProblem->upper - activeProblem->lower;

    double cellSize = width / HQ_GRID_SIZE;

    agent->position[0] = activeProblem->lower
        + (cellX + RandomDouble(0.0, 1.0))
            * cellSize;

    agent->position[1] = activeProblem->lower
        + (cellY + RandomDouble(0.0, 1.0))
            * cellSize;

    for (int d = 2; d < activeProblem->dim; d++) {
        agent->position[d] = RandomDouble(
            activeProblem->lower,
            activeProblem->upper);
    }

    agent->currentValue = Evaluate(agent->position);

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

static void StepAgent(Agent* agent)
{
    int first = GetDirection(agent, 0);

    int opposite = GetDirection(agent, 4);

    int perpendicular1 = GetDirection(agent, 2);

    int perpendicular2 = GetDirection(agent, 6);

    if (agent->phase == PHASE_FIRST) {
        TestDirection(agent, first);

        if (agent->localPoints[first].value < agent->currentValue) {
            MoveAgent(agent, first);
            return;
        }

        agent->phase = PHASE_OPPOSITE;
        return;
    }

    if (agent->phase == PHASE_OPPOSITE) {
        TestDirection(agent, opposite);

        if (agent->localPoints[opposite].value < agent->currentValue) {
            MoveAgent(agent, opposite);
            return;
        }

        agent->phase = PHASE_PERPENDICULAR_1;
        return;
    }

    if (agent->phase == PHASE_PERPENDICULAR_1) {
        TestDirection(
            agent,
            perpendicular1);

        agent->phase = PHASE_PERPENDICULAR_2;

        return;
    }

    if (agent->phase == PHASE_PERPENDICULAR_2) {
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

        /*
         * The centre was better than the first
         * four tested neighbours.
         *
         * Rotate the local cross by 45 degrees.
         * This examines the remaining four cells
         * of the 3 x 3 neighbourhood.
         */
        if (!agent->rotated) {
            RotateLocalSearch(agent);
            return;
        }

        /*
         * All eight neighbouring cells have now
         * been tested and the current position is
         * still the best.
         *
         * The agent reports "stuck" and HQ
         * immediately teleports it to a random
         * position.
         */
        TeleportAgentRandomly(agent);
    }
}

static void HeadquartersRedistribute(void)
{
    int density[HQ_GRID_SIZE][HQ_GRID_SIZE] = { 0 };
    double bestValue[HQ_GRID_SIZE][HQ_GRID_SIZE];

    double globalBest = INFINITY;
    double globalWorst = -INFINITY;

    double width = activeProblem->upper - activeProblem->lower;

    for (int y = 0; y < HQ_GRID_SIZE; y++) {
        for (int x = 0; x < HQ_GRID_SIZE; x++)
            bestValue[x][y] = INFINITY;
    }

    for (int i = 0; i < testedPointCount; i++) {
        int cellX = (int)((testedPoints[i].position[0]
                              - activeProblem->lower)
            / width
            * HQ_GRID_SIZE);

        int cellY = (int)((testedPoints[i].position[1]
                              - activeProblem->lower)
            / width
            * HQ_GRID_SIZE);

        if (cellX < 0)
            cellX = 0;

        if (cellX >= HQ_GRID_SIZE)
            cellX = HQ_GRID_SIZE - 1;

        if (cellY < 0)
            cellY = 0;

        if (cellY >= HQ_GRID_SIZE)
            cellY = HQ_GRID_SIZE - 1;

        density[cellX][cellY]++;

        if (testedPoints[i].value < bestValue[cellX][cellY])
            bestValue[cellX][cellY] = testedPoints[i].value;

        if (testedPoints[i].value < globalBest)
            globalBest = testedPoints[i].value;

        if (testedPoints[i].value > globalWorst)
            globalWorst = testedPoints[i].value;
    }

    int selectedAgents[AGENT_COUNT] = { 0 };

    for (int t = 0; t < HQ_TELEPORT_COUNT; t++) {
        int worstAgent = -1;
        double worstAgentValue = -INFINITY;

        for (int a = 0; a < AGENT_COUNT; a++) {
            if (selectedAgents[a])
                continue;

            if (agents[a].currentValue > worstAgentValue) {
                worstAgentValue = agents[a].currentValue;

                worstAgent = a;
            }
        }

        if (worstAgent < 0)
            break;

        selectedAgents[worstAgent] = 1;

        double highestScore = -INFINITY;

        int bestCellX = 0;
        int bestCellY = 0;

        int equalBestCount = 0;

        for (int y = 0; y < HQ_GRID_SIZE; y++) {
            for (int x = 0; x < HQ_GRID_SIZE; x++) {
                double exploration = 1.0
                    / (1.0 + density[x][y]);

                double quality = 0.0;

                if (density[x][y] > 0) {
                    double range = globalWorst - globalBest;

                    if (range > 0.0) {
                        quality = 1.0
                            - (bestValue[x][y] - globalBest)
                                / range;
                    } else
                        quality = 1.0;
                }

                double score = HQ_EXPLORATION_WEIGHT * exploration
                    + HQ_QUALITY_WEIGHT * quality;

                if (score > highestScore) {
                    highestScore = score;

                    bestCellX = x;
                    bestCellY = y;

                    equalBestCount = 1;
                } else if (
                    fabs(score - highestScore)
                    < 1e-12) {
                    equalBestCount++;

                    if (rand() % equalBestCount == 0) {
                        bestCellX = x;
                        bestCellY = y;
                    }
                }
            }
        }

        TeleportAgentToCell(
            &agents[worstAgent],
            bestCellX,
            bestCellY);

        density[bestCellX][bestCellY]++;
    }
}

static void DirectionalTreeInteractiveInit(
    const TestProblem* problem)
{
    activeProblem = problem;

    result.evaluations = 0;
    result.bestValue = INFINITY;

    testedPointCount = 0;
    hqStepCounter = 0;

    localStepSize = (problem->upper - problem->lower)
        * LOCAL_STEP_FRACTION;

    for (int a = 0; a < AGENT_COUNT; a++) {
        Agent* agent = &agents[a];

        for (int d = 0; d < problem->dim; d++) {
            agent->position[d] = RandomDouble(
                problem->lower,
                problem->upper);
        }

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

    hqStepCounter++;

    if (hqStepCounter >= HQ_INTERVAL) {
        HeadquartersRedistribute();
        hqStepCounter = 0;
    }
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

    if (point->tested)
        *type = DIRECTIONAL_TREE_INACTIVE_TYPE;
    else
        *type = DIRECTIONAL_TREE_ACTIVE_TYPE;
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
