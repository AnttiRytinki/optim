#include <float.h>
#include <math.h>
#include <stdlib.h>

#include "ScoutsInteractiveD.h"
#include "optimizers.h"

#define INITIAL_STEP_FRACTION 0.02
#define MIN_STEP_FRACTION 0.0000001
#define VALLEY_RADIUS_FRACTION 0.02
#define MAX_VALLEYS 100
#define RIDGE_SCAN_STEPS 32
#define RIDGE_STEP_FRACTION 0.02
#define PI 3.14159265358979323846
#define DIRECTION_COUNT 8
#define MAX_TESTED_POINTS 100000

#define SCOUT_AGENT_TYPE 5
#define SCOUT_ACTIVE_TYPE 6
#define SCOUT_INACTIVE_TYPE 7
#define SCOUT_TESTED_TYPE 8

typedef struct {
  double x;
  double y;
} Point;

static const double RelativeAngles[DIRECTION_COUNT] = {
    0.0, 180.0, 90.0, 270.0, 45.0, 135.0, 225.0, 315.0};

static const TestProblem *currentProblem;
static double agentX, agentY, agentValue, agentAngle;
static double stepSize, initialStep, minimumStep;
static double bestX, bestY, bestValue;
static int evaluations;
static Point testedPoints[MAX_TESTED_POINTS];
static int testedPointCount;
static int testedDirections[DIRECTION_COUNT];
static Point valleys[MAX_VALLEYS];
static int valleyCount;

/* The inherited ridge and the first point on its far side. */
static Point ridge;
static Point exitPoint;
static int crossingMode;
static int crossingPhase;

static double NormalizeAngle(double angle) {
  angle = fmod(angle, 2.0 * PI);
  if (angle < 0.0)
    angle += 2.0 * PI;
  return angle;
}

static double DistanceSquared(double x1, double y1, double x2, double y2) {
  double dx = x1 - x2;
  double dy = y1 - y2;
  return dx * dx + dy * dy;
}

static int InsideBounds(double x, double y) {
  return x >= currentProblem->lower && x <= currentProblem->upper &&
         y >= currentProblem->lower && y <= currentProblem->upper;
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

static void RestartRandomly(void) {
  agentX = RandomDouble(currentProblem->lower, currentProblem->upper);
  agentY = RandomDouble(currentProblem->lower, currentProblem->upper);
  agentAngle = RandomDouble(0.0, 2.0 * PI);
  stepSize = initialStep;
  crossingMode = 0;
  crossingPhase = 0;
  ResetTestedDirections();
  EvaluatePoint(agentX, agentY, &agentValue);
}

static int IsKnownValley(double x, double y) {
  double radius =
      (currentProblem->upper - currentProblem->lower) * VALLEY_RADIUS_FRACTION;
  for (int i = 0; i < valleyCount; i++) {
    if (DistanceSquared(x, y, valleys[i].x, valleys[i].y) < radius * radius)
      return 1;
  }
  return 0;
}

static void RememberValley(double x, double y) {
  if (IsKnownValley(x, y))
    return;

  if (valleyCount < MAX_VALLEYS) {
    valleys[valleyCount].x = x;
    valleys[valleyCount].y = y;
    valleyCount++;
  } else {
    /* Retain a bounded, rolling history. */
    for (int i = 1; i < MAX_VALLEYS; i++)
      valleys[i - 1] = valleys[i];
    valleys[MAX_VALLEYS - 1].x = x;
    valleys[MAX_VALLEYS - 1].y = y;
  }
}

/*
 * Scan rays out of the valley. A ridge is detected when the sampled
 * directional slope changes from positive to negative. Prefer rays whose
 * first downhill point beyond the ridge is outside previously visited valleys.
 * The original agent position is not changed during the scan.
 */
static int FindRidge(Point *foundRidge, Point *foundExit) {
  double spacing =
      (currentProblem->upper - currentProblem->lower) * RIDGE_STEP_FRACTION;
  double chosenScore = DBL_MAX;
  int found = 0;

  for (int direction = 0; direction < DIRECTION_COUNT; direction++) {
    double angle =
        NormalizeAngle(agentAngle + RelativeAngles[direction] * PI / 180.0);
    double dx = cos(angle);
    double dy = sin(angle);
    double previousValue = agentValue;
    double peakValue = agentValue;
    Point peak = {agentX, agentY};
    int rose = 0;

    for (int j = 1; j <= RIDGE_SCAN_STEPS; j++) {
      double x = agentX + j * spacing * dx;
      double y = agentY + j * spacing * dy;
      if (!InsideBounds(x, y))
        break;

      double value;
      EvaluatePoint(x, y, &value);

      if (value > previousValue) {
        rose = 1;
        peakValue = value;
        peak.x = x;
        peak.y = y;
      } else if (rose && value < previousValue) {
        /* First downhill sample after the detected crest. */
        double score = peakValue - agentValue;
        if (IsKnownValley(x, y))
          score += 1.0e9;

        if (score < chosenScore) {
          chosenScore = score;
          *foundRidge = peak;
          foundExit->x = x;
          foundExit->y = y;
          found = 1;
        }
        break;
      } else if (!rose) {
        /* Not a hill in this direction. */
        break;
      }
      previousValue = value;
    }
  }

  return found && chosenScore < 1.0e9;
}

static void DieAndReproduce(void) {
  Point nextRidge, nextExit;
  int repeated = IsKnownValley(agentX, agentY);
  RememberValley(agentX, agentY);

  if (!repeated && FindRidge(&nextRidge, &nextExit)) {
    ridge = nextRidge;
    exitPoint = nextExit;
    crossingMode = 1;
    crossingPhase = 0;
    stepSize = initialStep;
    agentAngle = atan2(ridge.y - agentY, ridge.x - agentX);
    ResetTestedDirections();
    /* The offspring is born at its parent's last position. */
    return;
  }

  RestartRandomly();
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
  valleyCount = 0;
  crossingMode = 0;
  crossingPhase = 0;
  ResetTestedDirections();
  RestartRandomly();
}

static int TestDirection(int direction, double *candidateX, double *candidateY,
                         double *candidateValue, double *candidateAngle) {
  double angle =
      NormalizeAngle(agentAngle + RelativeAngles[direction] * PI / 180.0);
  double x = agentX + stepSize * cos(angle);
  double y = agentY + stepSize * sin(angle);
  if (!InsideBounds(x, y))
    return 0;

  double value;
  EvaluatePoint(x, y, &value);
  testedDirections[direction] = 1;
  if (value >= *candidateValue)
    return 0;

  *candidateX = x;
  *candidateY = y;
  *candidateValue = value;
  *candidateAngle = angle;
  return 1;
}

static void ScoutsDStep(void) {
  ResetTestedDirections();

  if (crossingMode) {
    /* Cross the crest even when that requires temporarily going uphill. */
    Point target = crossingPhase == 0 ? ridge : exitPoint;
    double dx = target.x - agentX;
    double dy = target.y - agentY;
    double distance = sqrt(dx * dx + dy * dy);

    if (distance <= initialStep * 0.5) {
      if (crossingPhase == 0)
        crossingPhase = 1;
      else {
        crossingMode = 0;
        crossingPhase = 0;
        stepSize = initialStep;
      }
      return;
    }

    double move = distance < initialStep ? distance : initialStep;
    double x = agentX + move * dx / distance;
    double y = agentY + move * dy / distance;
    agentAngle = atan2(dy, dx);
    agentX = x;
    agentY = y;
    EvaluatePoint(agentX, agentY, &agentValue);
    return;
  }

  double candidateX = agentX;
  double candidateY = agentY;
  double candidateValue = agentValue;
  double candidateAngle = agentAngle;

  if (TestDirection(0, &candidateX, &candidateY, &candidateValue,
                    &candidateAngle))
    goto MoveAgent;
  if (TestDirection(1, &candidateX, &candidateY, &candidateValue,
                    &candidateAngle))
    goto MoveAgent;

  for (int i = 2; i < 4; i++)
    TestDirection(i, &candidateX, &candidateY, &candidateValue,
                  &candidateAngle);
  if (candidateValue < agentValue)
    goto MoveAgent;

  for (int i = 4; i < DIRECTION_COUNT; i++)
    TestDirection(i, &candidateX, &candidateY, &candidateValue,
                  &candidateAngle);
  if (candidateValue < agentValue)
    goto MoveAgent;

  stepSize *= 0.5;
  if (stepSize < minimumStep)
    DieAndReproduce();
  ResetTestedDirections();
  return;

MoveAgent:
  agentX = candidateX;
  agentY = candidateY;
  agentValue = candidateValue;
  agentAngle = candidateAngle;
  ResetTestedDirections();
}

static int ScoutsDGetPointCount(void) {
  return testedPointCount + 1 + DIRECTION_COUNT + (crossingMode ? 2 : 0);
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
  index--;

  if (index < DIRECTION_COUNT) {
    double angle =
        NormalizeAngle(agentAngle + RelativeAngles[index] * PI / 180.0);
    *x = agentX + stepSize * cos(angle);
    *y = agentY + stepSize * sin(angle);
    *type = testedDirections[index] ? SCOUT_INACTIVE_TYPE : SCOUT_ACTIVE_TYPE;
    return;
  }
  index -= DIRECTION_COUNT;
  Point p = index == 0 ? ridge : exitPoint;
  *x = p.x;
  *y = p.y;
  *type = SCOUT_ACTIVE_TYPE;
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
