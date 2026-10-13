
#include <float.h>
#include <math.h>
#include <stdlib.h>

#include "ScoutsInteractiveD.h"
#include "optimizers.h"

#define INITIAL_STEP_FRACTION 0.02
#define MIN_STEP_FRACTION 0.0000001
#define ESCAPE_STEPS 3

#define PI 3.14159265358979323846
#define DIRECTION_COUNT 8
#define MAX_TESTED_POINTS 100000

#define SCOUT_AGENT_TYPE 5
#define SCOUT_ACTIVE_TYPE 6
#define SCOUT_INACTIVE_TYPE 7
#define SCOUT_TESTED_TYPE 8

static const double RelativeAngles[DIRECTION_COUNT] = {
    0.0, 180.0, 90.0, 270.0, 45.0, 135.0, 225.0, 315.0};

typedef struct {
  double x;
  double y;
} TestedPoint;

static const TestProblem *currentProblem;

static double agentX;
static double agentY;
static double agentValue;
static double agentAngle;

static double stepSize;
static double initialStep;
static double minimumStep;

static double bestX;
static double bestY;
static double bestValue;

static int evaluations;

static int escapeMode;
static int escapeStepsRemaining;

static TestedPoint testedPoints[MAX_TESTED_POINTS];
static int testedPointCount;
static int testedDirections[DIRECTION_COUNT];

static double NormalizeAngle(double angle) {
  angle = fmod(angle, 2.0 * PI);

  if (angle < 0.0)
    angle += 2.0 * PI;

  return angle;
}

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

static void ScoutsDInit(const TestProblem *problem) {
  currentProblem = problem;

  initialStep = (problem->upper - problem->lower) * INITIAL_STEP_FRACTION;
  minimumStep = (problem->upper - problem->lower) * MIN_STEP_FRACTION;

  bestValue = DBL_MAX;
  bestX = 0.0;
  bestY = 0.0;
  evaluations = 0;

  testedPointCount = 0;
  ResetTestedDirections();

  escapeMode = 0;
  escapeStepsRemaining = 0;

  agentX = RandomDouble(problem->lower, problem->upper);
  agentY = RandomDouble(problem->lower, problem->upper);
  agentAngle = RandomDouble(0.0, 2.0 * PI);

  stepSize = initialStep;

  EvaluatePoint(agentX, agentY, &agentValue);
}

static int IsImprovement(double value, double reference) {
  if (escapeMode)
    return value > reference;

  return value < reference;
}

static int TestDirection(int direction, double *candidateX, double *candidateY,
                         double *candidateValue, double *candidateAngle) {
  double angle =
      NormalizeAngle(agentAngle + RelativeAngles[direction] * PI / 180.0);

  double x = agentX + stepSize * cos(angle);
  double y = agentY + stepSize * sin(angle);

  if (x < currentProblem->lower || x > currentProblem->upper ||
      y < currentProblem->lower || y > currentProblem->upper)
    return 0;

  double value;

  EvaluatePoint(x, y, &value);
  testedDirections[direction] = 1;

  if (!IsImprovement(value, *candidateValue))
    return 0;

  *candidateX = x;
  *candidateY = y;
  *candidateValue = value;
  *candidateAngle = angle;

  return 1;
}

static void StartEscape(void) {
  escapeMode = 1;
  escapeStepsRemaining = ESCAPE_STEPS;
  stepSize = initialStep;
}

static void FinishEscapeStep(void) {
  if (!escapeMode)
    return;

  escapeStepsRemaining--;

  if (escapeStepsRemaining <= 0) {
    escapeMode = 0;
    escapeStepsRemaining = 0;
    stepSize = initialStep;
  }
}

static void ScoutsDStep(void) {
  double candidateX = agentX;
  double candidateY = agentY;
  double candidateValue = agentValue;
  double candidateAngle = agentAngle;

  ResetTestedDirections();

  // Priority 1: straight ahead
  if (TestDirection(0, &candidateX, &candidateY, &candidateValue,
                    &candidateAngle))
    goto MoveAgent;

  // Priority 2: turn around
  if (TestDirection(1, &candidateX, &candidateY, &candidateValue,
                    &candidateAngle))
    goto MoveAgent;

  // Priority 3: perpendicular directions
  for (int i = 2; i < 4; i++)
    TestDirection(i, &candidateX, &candidateY, &candidateValue,
                  &candidateAngle);

  if (IsImprovement(candidateValue, agentValue))
    goto MoveAgent;

  // Priority 4: diagonal directions
  for (int i = 4; i < DIRECTION_COUNT; i++)
    TestDirection(i, &candidateX, &candidateY, &candidateValue,
                  &candidateAngle);

  if (IsImprovement(candidateValue, agentValue))
    goto MoveAgent;

  // No improvement in the current search direction.
  stepSize *= 0.5;

  if (!escapeMode && stepSize < minimumStep)
    StartEscape();

  FinishEscapeStep();
  ResetTestedDirections();
  return;

MoveAgent:
  agentX = candidateX;
  agentY = candidateY;
  agentValue = candidateValue;
  agentAngle = candidateAngle;

  FinishEscapeStep();
  ResetTestedDirections();
}

static int ScoutsDGetPointCount(void) {
  return testedPointCount + 1 + DIRECTION_COUNT;
}

static void ScoutsDGetPoint(int index, double *x, double *y, int *type) {
  if (index < 0 || index >= ScoutsDGetPointCount())
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

  double angle =
      NormalizeAngle(agentAngle + RelativeAngles[direction] * PI / 180.0);

  *x = agentX + stepSize * cos(angle);
  *y = agentY + stepSize * sin(angle);

  *type = testedDirections[direction] ? SCOUT_INACTIVE_TYPE : SCOUT_ACTIVE_TYPE;
}

static void ScoutsDGetBest(double *x, double *y, double *value, int *count) {
  *x = bestX;
  *y = bestY;
  *value = bestValue;
  *count = evaluations;
}

InteractiveOptimizer ScoutsDOptimizer = {"ScoutsD",       ScoutsDInit,
                                         ScoutsDStep,     ScoutsDGetPointCount,
                                         ScoutsDGetPoint, ScoutsDGetBest};
