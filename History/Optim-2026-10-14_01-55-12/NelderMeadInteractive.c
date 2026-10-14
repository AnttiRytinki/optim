
#include <float.h>
#include <math.h>
#include <stdlib.h>

#include "NelderMeadInteractive.h"
#include "optimizers.h"

#define SIMPLEX_SIZE 3
#define MAX_TESTED_POINTS 100000

#define INITIAL_STEP_FRACTION 0.02
#define MIN_STEP_FRACTION 0.0000001

#define REFLECTION 1.0
#define EXPANSION 2.0
#define CONTRACTION 0.5
#define SHRINK 0.5

#define AGENT_TYPE 5
#define ACTIVE_TYPE 6
#define TESTED_TYPE 8

typedef struct {
  double x;
  double y;
  double value;
} Point;

static const TestProblem *currentProblem;

static Point simplex[SIMPLEX_SIZE];
static Point testedPoints[MAX_TESTED_POINTS];

static int testedPointCount;
static int evaluations;

static double initialStep;
static double minimumStep;

static double bestX;
static double bestY;
static double bestValue;

static double Clamp(double value) {
  if (value < currentProblem->lower)
    return currentProblem->lower;

  if (value > currentProblem->upper)
    return currentProblem->upper;

  return value;
}

static Point Evaluate(double x, double y) {
  Point point;

  point.x = Clamp(x);
  point.y = Clamp(y);

  double position[MAX_DIM] = {0.0};

  position[0] = point.x;
  position[1] = point.y;

  for (int d = 2; d < currentProblem->dim; d++)
    position[d] = RandomDouble(currentProblem->lower, currentProblem->upper);

  point.value = currentProblem->function(position, currentProblem->dim);
  evaluations++;

  if (testedPointCount < MAX_TESTED_POINTS)
    testedPoints[testedPointCount++] = point;

  if (point.value < bestValue) {
    bestValue = point.value;
    bestX = point.x;
    bestY = point.y;
  }

  return point;
}

static void SortSimplex(void) {
  for (int i = 0; i < SIMPLEX_SIZE - 1; i++) {
    for (int j = i + 1; j < SIMPLEX_SIZE; j++) {
      if (simplex[j].value < simplex[i].value) {
        Point temp = simplex[i];
        simplex[i] = simplex[j];
        simplex[j] = temp;
      }
    }
  }
}

static void RestartSimplex(void) {
  double x = RandomDouble(currentProblem->lower, currentProblem->upper);
  double y = RandomDouble(currentProblem->lower, currentProblem->upper);

  double angle = RandomDouble(0.0, 6.28318530717958647692);

  simplex[0] = Evaluate(x, y);

  simplex[1] =
      Evaluate(x + initialStep * cos(angle), y + initialStep * sin(angle));

  simplex[2] = Evaluate(x + initialStep * cos(angle + 2.09439510239319549),
                        y + initialStep * sin(angle + 2.09439510239319549));

  SortSimplex();
}

static void NelderMeadInit(const TestProblem *problem) {
  currentProblem = problem;

  initialStep = (problem->upper - problem->lower) * INITIAL_STEP_FRACTION;
  minimumStep = (problem->upper - problem->lower) * MIN_STEP_FRACTION;

  evaluations = 0;
  testedPointCount = 0;

  bestX = 0.0;
  bestY = 0.0;
  bestValue = DBL_MAX;

  RestartSimplex();
}

static void NelderMeadStep(void) {
  SortSimplex();

  double dx1 = simplex[1].x - simplex[0].x;
  double dy1 = simplex[1].y - simplex[0].y;
  double dx2 = simplex[2].x - simplex[0].x;
  double dy2 = simplex[2].y - simplex[0].y;

  double diameter1 = hypot(dx1, dy1);
  double diameter2 = hypot(dx2, dy2);

  if (fmax(diameter1, diameter2) < minimumStep) {
    RestartSimplex();
    return;
  }

  double cx = (simplex[0].x + simplex[1].x) * 0.5;
  double cy = (simplex[0].y + simplex[1].y) * 0.5;

  Point reflected = Evaluate(cx + REFLECTION * (cx - simplex[2].x),
                             cy + REFLECTION * (cy - simplex[2].y));

  if (reflected.value < simplex[0].value) {
    Point expanded = Evaluate(cx + EXPANSION * (reflected.x - cx),
                              cy + EXPANSION * (reflected.y - cy));

    simplex[2] = expanded.value < reflected.value ? expanded : reflected;
    return;
  }

  if (reflected.value < simplex[1].value) {
    simplex[2] = reflected;
    return;
  }

  Point contracted;

  if (reflected.value < simplex[2].value) {
    contracted = Evaluate(cx + CONTRACTION * (reflected.x - cx),
                          cy + CONTRACTION * (reflected.y - cy));

    if (contracted.value <= reflected.value) {
      simplex[2] = contracted;
      return;
    }
  } else {
    contracted = Evaluate(cx + CONTRACTION * (simplex[2].x - cx),
                          cy + CONTRACTION * (simplex[2].y - cy));

    if (contracted.value < simplex[2].value) {
      simplex[2] = contracted;
      return;
    }
  }

  // Shrink the simplex towards its best vertex.
  for (int i = 1; i < SIMPLEX_SIZE; i++) {
    double x = simplex[0].x + SHRINK * (simplex[i].x - simplex[0].x);
    double y = simplex[0].y + SHRINK * (simplex[i].y - simplex[0].y);

    simplex[i] = Evaluate(x, y);
  }
}

static int NelderMeadGetPointCount(void) {
  return testedPointCount + SIMPLEX_SIZE;
}

static void NelderMeadGetPoint(int index, double *x, double *y, int *type) {
  if (index < 0 || index >= NelderMeadGetPointCount())
    return;

  if (index < testedPointCount) {
    *x = testedPoints[index].x;
    *y = testedPoints[index].y;
    *type = TESTED_TYPE;
    return;
  }

  index -= testedPointCount;

  *x = simplex[index].x;
  *y = simplex[index].y;

  *type = index == 0 ? AGENT_TYPE : ACTIVE_TYPE;
}

static void NelderMeadGetBest(double *x, double *y, double *value, int *count) {
  *x = bestX;
  *y = bestY;
  *value = bestValue;
  *count = evaluations;
}

InteractiveOptimizer NelderMeadOptimizer = {
    "NelderMead",       NelderMeadInit,
    NelderMeadStep,     NelderMeadGetPointCount,
    NelderMeadGetPoint, NelderMeadGetBest};
