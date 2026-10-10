
#include <float.h>
#include <stdlib.h>

#include "ScoutsInteractiveA.h"
#include "optimizers.h"

#define INITIAL_STEP_FRACTION 0.02
#define MIN_STEP_FRACTION 0.0000001

#define DIRECTION_COUNT 8
#define MAX_TESTED_POINTS 100000

#define SCOUT_AGENT_TYPE 5
#define SCOUT_ACTIVE_TYPE 6
#define SCOUT_INACTIVE_TYPE 7
#define SCOUT_TESTED_TYPE 8

static const int DirectionX[DIRECTION_COUNT] = {0, 1, 1, 1, 0, -1, -1, -1};
static const int DirectionY[DIRECTION_COUNT] = {-1, -1, 0, 1, 1, 1, 0, -1};

typedef struct {
  double x;
  double y;
} TestedPoint;

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

static TestedPoint testedPoints[MAX_TESTED_POINTS];
static int testedPointCount;
static int testedDirections[DIRECTION_COUNT];

static void ResetTestedDirections(void) {
  for (int i = 0; i < DIRECTION_COUNT; i++)
    testedDirections[i] = 0;
}

static void EvaluatePoint(double x, double y, double *value) {
  double position[MAX_DIM] = {0.0};

  position[0] = x;
  position[1] = y;

  for (int d = 2; d < currentProblem->dim; d++)
    position[d] = RandomDouble(currentProblem->lower, currentProblem->upper);

  *value = currentProblem->function(position, currentProblem->dim);
  evaluations++;

  if (testedPointCount < MAX_TESTED_POINTS) {
    testedPoints[testedPointCount].x = x;
    testedPoints[testedPointCount].y = y;
    testedPointCount++;
  }

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

  ResetTestedDirections();

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

  testedPointCount = 0;
  ResetTestedDirections();

  RestartAgent();
}

static void ScoutsAStep(void) {
  double candidateX = agentX;
  double candidateY = agentY;
  double candidateValue = agentValue;

  ResetTestedDirections();

  for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
    double x = agentX + DirectionX[direction] * stepSize;
    double y = agentY + DirectionY[direction] * stepSize;

    if (x < currentProblem->lower || x > currentProblem->upper ||
        y < currentProblem->lower || y > currentProblem->upper)
      continue;

    double value;

    EvaluatePoint(x, y, &value);
    testedDirections[direction] = 1;

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

  ResetTestedDirections();
}

static int ScoutsAGetPointCount(void) {
  return testedPointCount + 1 + DIRECTION_COUNT;
}

static void ScoutsAGetPoint(int index, double *x, double *y, int *type) {
  if (index < 0 || index >= ScoutsAGetPointCount())
    return;

  if (index < testedPointCount) {
    *x = testedPoints[index].x;
    *y = testedPoints[index].y;
    *type = SCOUT_TESTED_TYPE;
    return;
  }

  index -= testedPointCount;

  if (index == 0) {
    *x = agentX;
    *y = agentY;
    *type = SCOUT_AGENT_TYPE;
    return;
  }

  int direction = index - 1;

  *x = agentX + DirectionX[direction] * stepSize;
  *y = agentY + DirectionY[direction] * stepSize;

  *type = testedDirections[direction] ? SCOUT_INACTIVE_TYPE : SCOUT_ACTIVE_TYPE;
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
