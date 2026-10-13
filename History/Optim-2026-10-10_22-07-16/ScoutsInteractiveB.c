#include <float.h>
#include <math.h>
#include <stdlib.h>

#include "ScoutsInteractiveB.h"
#include "optimizers.h"

#define INITIAL_STEP_FRACTION 0.02
#define MIN_STEP_FRACTION 0.0000001

#define PI 3.14159265358979323846
#define DIRECTION_COUNT 8

static const double RelativeAngles[DIRECTION_COUNT] = {
    0.0, 180.0, 90.0, 270.0, 45.0, 135.0, 225.0, 315.0};

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

static double NormalizeAngle(double angle) {
  angle = fmod(angle, 2.0 * PI);

  if (angle < 0.0)
    angle += 2.0 * PI;

  return angle;
}

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

  agentAngle = RandomDouble(0.0, 2.0 * PI);
  stepSize = initialStep;

  EvaluatePoint(agentX, agentY, &agentValue);
}

static void ScoutsBInit(const TestProblem *problem) {
  currentProblem = problem;

  initialStep = (problem->upper - problem->lower) * INITIAL_STEP_FRACTION;
  minimumStep = (problem->upper - problem->lower) * MIN_STEP_FRACTION;

  bestValue = DBL_MAX;
  bestX = 0.0;
  bestY = 0.0;
  evaluations = 0;

  RestartAgent();
}

static int TestDirection(double relativeAngle, double *candidateX,
                         double *candidateY, double *candidateValue,
                         double *candidateAngle) {
  double angle = NormalizeAngle(agentAngle + relativeAngle * PI / 180.0);

  double x = agentX + stepSize * cos(angle);
  double y = agentY + stepSize * sin(angle);

  if (x < currentProblem->lower || x > currentProblem->upper ||
      y < currentProblem->lower || y > currentProblem->upper)
    return 0;

  double value;

  EvaluatePoint(x, y, &value);

  if (value >= *candidateValue)
    return 0;

  *candidateX = x;
  *candidateY = y;
  *candidateValue = value;
  *candidateAngle = angle;

  return 1;
}

static void ScoutsBStep(void) {
  double candidateX = agentX;
  double candidateY = agentY;
  double candidateValue = agentValue;
  double candidateAngle = agentAngle;

  // Priority 1: straight ahead
  if (TestDirection(RelativeAngles[0], &candidateX, &candidateY,
                    &candidateValue, &candidateAngle))
    goto MoveAgent;

  // Priority 2: turn around
  if (TestDirection(RelativeAngles[1], &candidateX, &candidateY,
                    &candidateValue, &candidateAngle))
    goto MoveAgent;

  // Priority 3: perpendicular directions
  for (int i = 2; i < 4; i++)
    TestDirection(RelativeAngles[i], &candidateX, &candidateY, &candidateValue,
                  &candidateAngle);

  if (candidateValue < agentValue)
    goto MoveAgent;

  // Priority 4: diagonal directions
  for (int i = 4; i < DIRECTION_COUNT; i++)
    TestDirection(RelativeAngles[i], &candidateX, &candidateY, &candidateValue,
                  &candidateAngle);

  if (candidateValue < agentValue)
    goto MoveAgent;

  // No improvement: reduce step size, preserve heading
  stepSize *= 0.5;

  if (stepSize < minimumStep)
    RestartAgent();

  return;

MoveAgent:
  agentX = candidateX;
  agentY = candidateY;
  agentValue = candidateValue;
  agentAngle = candidateAngle;
}

static int ScoutsBGetPointCount(void) { return 1; }

static void ScoutsBGetPoint(int index, double *x, double *y, int *type) {
  if (index != 0)
    return;

  *x = agentX;
  *y = agentY;
  *type = 0;
}

static void ScoutsBGetBest(double *x, double *y, double *value, int *count) {
  *x = bestX;
  *y = bestY;
  *value = bestValue;
  *count = evaluations;
}

InteractiveOptimizer ScoutsBOptimizer = {"ScoutsB",       ScoutsBInit,
                                         ScoutsBStep,     ScoutsBGetPointCount,
                                         ScoutsBGetPoint, ScoutsBGetBest};
