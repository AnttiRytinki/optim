#include <math.h>
#include <stdlib.h>

#include "optimizers.h"

#define AGENT_COUNT 8
#define DIRECTION_COUNT 8
#define MAX_TESTED_POINTS 100000

#define DIRECTIONAL_TREE_AGENT_TYPE 5
#define DIRECTIONAL_TREE_ACTIVE_TYPE 6
#define DIRECTIONAL_TREE_INACTIVE_TYPE 7
#define DIRECTIONAL_TREE_TESTED_TYPE 8

typedef struct
{
    double direction[MAX_DIM];
    double position[MAX_DIM];
    double value;
    int active;
} DirectionCandidate;

typedef struct
{
    double position[MAX_DIM];
    double currentValue;
    double stepSize;
    int searchDepth;
    DirectionCandidate candidates[DIRECTION_COUNT];
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

static void InitializeDirections(Agent* agent)
{
    for (int i = 0; i < DIRECTION_COUNT; i++) {
        double angle = 2.0 * M_PI * (double)i / (double)DIRECTION_COUNT;

        for (int d = 0; d < activeProblem->dim; d++)
            agent->candidates[i].direction[d] = 0.0;

        agent->candidates[i].direction[0] = cos(angle);
        agent->candidates[i].direction[1] = sin(angle);
    }
}

static void StartSearch(Agent* agent)
{
    agent->searchDepth = 1;

    for (int i = 0; i < DIRECTION_COUNT; i++) {
        agent->candidates[i].active = 1;
        agent->candidates[i].value = INFINITY;

        CopyVector(
            agent->candidates[i].position,
            agent->position,
            activeProblem->dim);
    }
}

static int CountActive(const Agent* agent)
{
    int count = 0;

    for (int i = 0; i < DIRECTION_COUNT; i++) {
        if (agent->candidates[i].active)
            count++;
    }

    return count;
}

static void EvaluateCandidate(
    Agent* agent,
    DirectionCandidate* candidate)
{
    for (int d = 0; d < activeProblem->dim; d++) {
        candidate->position[d] = agent->position[d]
            + candidate->direction[d]
                * agent->stepSize
                * agent->searchDepth;
    }

    Clamp(candidate->position, activeProblem);

    candidate->value = Evaluate(candidate->position);
}

static void RemoveWorstHalf(Agent* agent)
{
    int activeCount = CountActive(agent);

    if (activeCount <= 1)
        return;

    int removeCount = activeCount / 2;

    for (int r = 0; r < removeCount; r++) {
        int worstIndex = -1;
        double worstValue = -INFINITY;

        for (int i = 0; i < DIRECTION_COUNT; i++) {
            if (!agent->candidates[i].active)
                continue;

            if (agent->candidates[i].value > worstValue) {
                worstValue = agent->candidates[i].value;
                worstIndex = i;
            }
        }

        if (worstIndex >= 0)
            agent->candidates[worstIndex].active = 0;
    }
}

static int GetRemainingCandidate(const Agent* agent)
{
    for (int i = 0; i < DIRECTION_COUNT; i++) {
        if (agent->candidates[i].active)
            return i;
    }

    return -1;
}

static void FinishSearch(Agent* agent)
{
    int winner = GetRemainingCandidate(agent);

    if (winner < 0) {
        StartSearch(agent);
        return;
    }

    DirectionCandidate* candidate = &agent->candidates[winner];

    if (candidate->value < agent->currentValue) {
        CopyVector(
            agent->position,
            candidate->position,
            activeProblem->dim);

        agent->currentValue = candidate->value;
    } else
        agent->stepSize *= 0.5;

    double minimumStep = (activeProblem->upper - activeProblem->lower)
        * 0.000001;

    if (agent->stepSize < minimumStep) {
        agent->stepSize = (activeProblem->upper - activeProblem->lower)
            * 0.05;
    }

    StartSearch(agent);
}

static void StepAgent(Agent* agent)
{
    int activeCount = CountActive(agent);

    if (activeCount <= 1) {
        FinishSearch(agent);
        return;
    }

    for (int i = 0; i < DIRECTION_COUNT; i++) {
        if (!agent->candidates[i].active)
            continue;

        EvaluateCandidate(
            agent,
            &agent->candidates[i]);
    }

    RemoveWorstHalf(agent);

    agent->searchDepth++;
}

static void DirectionalTreeInteractiveInit(
    const TestProblem* problem)
{
    activeProblem = problem;

    result.evaluations = 0;
    result.bestValue = INFINITY;

    testedPointCount = 0;

    for (int a = 0; a < AGENT_COUNT; a++) {
        Agent* agent = &agents[a];

        for (int d = 0; d < problem->dim; d++) {
            agent->position[d] = RandomDouble(
                problem->lower,
                problem->upper);
        }

        agent->currentValue = Evaluate(agent->position);

        agent->stepSize = (problem->upper - problem->lower)
            * 0.05;

        InitializeDirections(agent);
        StartSearch(agent);
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

    int candidateIndex = index % DIRECTION_COUNT;

    DirectionCandidate* candidate = &agents[agentIndex].candidates[candidateIndex];

    *x = candidate->position[0];
    *y = candidate->position[1];

    if (candidate->active)
        *type = DIRECTIONAL_TREE_ACTIVE_TYPE;
    else
        *type = DIRECTIONAL_TREE_INACTIVE_TYPE;
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
