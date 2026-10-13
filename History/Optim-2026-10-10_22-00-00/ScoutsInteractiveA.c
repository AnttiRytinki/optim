#include <float.h>
#include <stdlib.h>

#include "ScoutsInteractiveA.h"
#include "optimizers.h"

#define INITIAL_STEP_FRACTION 0.02
#define MIN_STEP_FRACTION 0.0000001

#define DIRECTION_COUNT 8

static const int DirectionX[DIRECTION_COUNT] = {0, 1, 1, 1, 0, -1, -1, -1};

static const int DirectionY[DIRECTION_COUNT] = {-1, -1, 0, 1, 1, 1, 0, -1};

static const TestProblem *currentProblem;

static double agentX;
static double agentY;
static double agentValue;
static double stepSize;
static double initialStep;
static double minimumStep;

static double bestX;
static double bestY;
static double bestValue;

static int evaluations;

static void EvaluatePoint(double x, double y, double *value) {
  double position[MAX_DIM] = {0.0};

  position[0] = x;
  position[1] = y;

  for (int d = 2; d < currentProblem->dim; d++)
    position[d] = RandomDouble(currentProblem->lower, currentProblem->upper);

  *value = currentProblem->function(position, currentProblem->dim);
  evaluations++;

  if (*value < bestValue) {
    bestValue = *value;
    bestX = x;
    bestY = y;
  }
}

static void RestartAgent(void) {
  agentX = RandomDouble(currentProblem->lower, currentProblem->upper);
  agentY = RandomDouble(currentProblem->lower, currentProblem->upper);

  stepSize = initialStep;

  EvaluatePoint(agentX, agentY, &agentValue);
}

static void ScoutsAInit(const TestProblem *problem) {
  currentProblem = problem;

  initialStep = (problem->upper - problem->lower) * INITIAL_STEP_FRACTION;
  minimumStep = (problem->upper - problem->lower) * MIN_STEP_FRACTION;

  bestValue = DBL_MAX;
  bestX = 0.0;
  bestY = 0.0;
  evaluations = 0;

  RestartAgent();
}

static void ScoutsAStep(void) {
  double candidateX = agentX;
  double candidateY = agentY;
  double candidateValue = agentValue;

  for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
    double x = agentX + DirectionX[direction] * stepSize;
    double y = agentY + DirectionY[direction] * stepSize;

    if (x < currentProblem->lower || x > currentProblem->upper ||
        y < currentProblem->lower || y > currentProblem->upper)
      continue;

    double value;

    EvaluatePoint(x, y, &value);

    if (value < candidateValue) {
      candidateValue = value;
      candidateX = x;
      candidateY = y;
    }
  }

  if (candidateValue < agentValue) {
    agentX = candidateX;
    agentY = candidateY;
    agentValue = candidateValue;
  } else {
    stepSize *= 0.5;

    if (stepSize < minimumStep)
      RestartAgent();
  }
}

static int ScoutsAGetPointCount(void) { return 1; }

static void ScoutsAGetPoint(int index, double *x, double *y, int *type) {
  if (index != 0)
    return;

  *x = agentX;
  *y = agentY;
  *type = 0;
}

static void ScoutsAGetBest(double *x, double *y, double *value, int *count) {
  *x = bestX;
  *y = bestY;
  *value = bestValue;
  *count = evaluations;
}

InteractiveOptimizer ScoutsAOptimizer = {"ScoutsA",       ScoutsAInit,
                                         ScoutsAStep,     ScoutsAGetPointCount,
                                         ScoutsAGetPoint, ScoutsAGetBest};
