#include <math.h>
#include <stdlib.h>

#include "optimizers.h"

#define AGENT_COUNT 8
#define DIRECTION_COUNT 8
#define MAX_TESTED_POINTS 100000

#define MAX_STEP_FRACTION 0.02
#define MIN_STEP_FRACTION 0.000001

#define REFINER_COUNT 2

#define HQ_CANDIDATE_COUNT 512

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
    double x;
    double y;
    double clearance;
} HQCandidate;

static const TestProblem* activeProblem;
static AlgorithmResult result;

static Agent agents[AGENT_COUNT];

static TestedPoint testedPoints[MAX_TESTED_POINTS];
static int testedPointCount;

static HQCandidate hqCandidates[HQ_CANDIDATE_COUNT];

static int globalCoverageComplete;

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

static double GetMaximumStep(void)
{
    return (activeProblem->upper
               - activeProblem->lower)
        * MAX_STEP_FRACTION;
}

static double GetMinimumStep(void)
{
    return (activeProblem->upper
               - activeProblem->lower)
        * MIN_STEP_FRACTION;
}

static double Halton(int index, int base)
{
    double result = 0.0;
    double fraction = 1.0;

    while (index > 0) {
        fraction /= base;

        result += fraction
            * (index % base);

        index /= base;
    }

    return result;
}

static void InitializeHQCandidates(void)
{
    double lower = activeProblem->lower;

    double upper = activeProblem->upper;

    double width = upper - lower;

    for (int i = 0; i < HQ_CANDIDATE_COUNT; i++) {
        double hx = Halton(i + 1, 2);

        double hy = Halton(i + 1, 3);

        hqCandidates[i].x = lower + hx * width;

        hqCandidates[i].y = lower + hy * width;

        /*
         * Initial clearance is limited only
         * by the boundaries of the search area.
         */
        hqCandidates[i].clearance = fmin(
            fmin(
                hqCandidates[i].x - lower,
                upper - hqCandidates[i].x),
            fmin(
                hqCandidates[i].y - lower,
                upper - hqCandidates[i].y));
    }
}

static void UpdateHQCoverage(
    const double* position)
{
    if (globalCoverageComplete)
        return;

    for (int i = 0; i < HQ_CANDIDATE_COUNT; i++) {
        double dx = fabs(
            hqCandidates[i].x
            - position[0]);

        double dy = fabs(
            hqCandidates[i].y
            - position[1]);

        /*
         * Chebyshev distance corresponds to
         * an axis-aligned empty square.
         */
        double distance = fmax(dx, dy);

        if (distance < hqCandidates[i].clearance) {
            hqCandidates[i].clearance = distance;
        }
    }
}

static void RegisterTestedPoint(
    const double* position,
    double value)
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

    RegisterTestedPoint(
        position,
        value);

    UpdateHQCoverage(position);

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
    /*
     * Rotating the discrete cross by one
     * direction means 45 degrees.
     *
     * This is not arbitrary direction rotation.
     * It simply switches from the cardinal cross
     * to the diagonal cross, or vice versa.
     */
    agent->baseDirection = (agent->baseDirection + 1)
        % DIRECTION_COUNT;

    agent->rotated = 1;
    agent->phase = PHASE_FIRST;
}

static int GetLargestHoleCandidate(
    double* x,
    double* y,
    double* clearance)
{
    if (globalCoverageComplete)
        return 0;

    int bestIndex = -1;
    double bestClearance = -INFINITY;

    for (int i = 0; i < HQ_CANDIDATE_COUNT; i++) {
        if (
            hqCandidates[i].clearance
            > bestClearance) {
            bestClearance = hqCandidates[i].clearance;

            bestIndex = i;
        }
    }

    if (bestIndex < 0)
        return 0;

    *x = hqCandidates[bestIndex].x;

    *y = hqCandidates[bestIndex].y;

    *clearance = hqCandidates[bestIndex].clearance;

    return 1;
}

static void TeleportAgentRandomly(
    Agent* agent)
{
    for (int d = 0; d < activeProblem->dim; d++) {
        agent->position[d] = RandomDouble(
            activeProblem->lower,
            activeProblem->upper);
    }

    agent->stepSize = GetMaximumStep();

    agent->currentValue = Evaluate(agent->position);

    StartLocalSearch(agent);
}

static void TeleportAgentForExploration(
    Agent* agent)
{
    if (globalCoverageComplete) {
        TeleportAgentRandomly(agent);
        return;
    }

    double x;
    double y;
    double clearance;

    if (!GetLargestHoleCandidate(
            &x,
            &y,
            &clearance)) {
        globalCoverageComplete = 1;

        TeleportAgentRandomly(agent);
        return;
    }

    /*
     * Once the largest remaining hole is no
     * larger than an agent's maximum local step,
     * coarse global coverage is considered done.
     *
     * HQ then permanently stops maintaining and
     * searching the coverage candidates.
     */
    if (clearance <= GetMaximumStep()) {
        globalCoverageComplete = 1;

        TeleportAgentRandomly(agent);
        return;
    }

    agent->position[0] = x;

    agent->position[1] = y;

    for (int d = 2; d < activeProblem->dim; d++) {
        agent->position[d] = RandomDouble(
            activeProblem->lower,
            activeProblem->upper);
    }

    agent->stepSize = GetMaximumStep();

    agent->currentValue = Evaluate(agent->position);

    StartLocalSearch(agent);
}

static int ShouldRefine(
    const Agent* agent)
{
    int betterAgents = 0;

    for (int i = 0; i < AGENT_COUNT; i++) {
        if (&agents[i] == agent)
            continue;

        if (
            agents[i].currentValue
            < agent->currentValue) {
            betterAgents++;
        }
    }

    return betterAgents < REFINER_COUNT;
}

static void RefineAgent(
    Agent* agent)
{
    agent->stepSize *= 0.5;

    double minimumStep = GetMinimumStep();

    if (agent->stepSize < minimumStep)
        agent->stepSize = minimumStep;

    StartLocalSearch(agent);
}

static void HandleStuckAgent(
    Agent* agent)
{
    if (
        ShouldRefine(agent)
        && agent->stepSize > GetMinimumStep()) {
        RefineAgent(agent);
        return;
    }

    TeleportAgentForExploration(agent);
}

static void StepAgent(
    Agent* agent)
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
         * All eight neighbours have been tested.
         * The agent is at the lowest point of its
         * current 3 x 3 neighbourhood.
         */
        HandleStuckAgent(agent);
    }
}

static void DirectionalTreeInteractiveInit(
    const TestProblem* problem)
{
    activeProblem = problem;

    result.evaluations = 0;
    result.bestValue = INFINITY;

    testedPointCount = 0;

    globalCoverageComplete = 0;

    InitializeHQCandidates();

    for (int a = 0; a < AGENT_COUNT; a++) {
        Agent* agent = &agents[a];

        for (int d = 0; d < problem->dim; d++) {
            agent->position[d] = RandomDouble(
                problem->lower,
                problem->upper);
        }

        agent->stepSize = GetMaximumStep();

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
