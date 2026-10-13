#include <float.h>
#include <math.h>
#include <stdlib.h>

#include "ScoutsInteractiveD.h"

#define ANTI_AGENT_COUNT 5
#define INITIAL_STEP_FRACTION 0.02
#define MIN_STEP_FRACTION 0.0000001
#define TERRITORY_RADIUS_FRACTION 0.01
#define MAX_EVALUATIONS 100000
#define SPAWN_ATTEMPTS 1000
#define PI 3.14159265358979323846
#define DIRECTION_COUNT 8

static const double RelativeAngles[DIRECTION_COUNT] = {
    0.0, 180.0, 90.0, 270.0, 45.0, 135.0, 225.0, 315.0};

typedef struct {
  double x;
  double y;
} Point;

typedef struct {
  double x;
  double y;
  double value;
  double angle;
  double step;
  int anti;
} Agent;

static const TestProblem *currentProblem;
static Agent agents[ANTI_AGENT_COUNT + 1];
static int nextAgent;
static int evaluations;
static double initialStep;
static double minimumStep;
static double territoryRadiusSquared;
static double bestX;
static double bestY;
static double bestValue;
static Point history[MAX_EVALUATIONS];
static int historyCount;
static Point territory[MAX_EVALUATIONS];
static int territoryCount;

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

static double NearestTerritoryDistanceSquared(double x, double y) {
  double nearest = DBL_MAX;
  for (int i = 0; i < territoryCount; i++) {
    double distance = DistanceSquared(x, y, territory[i].x, territory[i].y);
    if (distance < nearest)
      nearest = distance;
  }
  return nearest;
}

static void ChooseNormalSpawn(double *x, double *y) {
  double fallbackX = currentProblem->lower;
  double fallbackY = currentProblem->lower;
  double fallbackDistance = -1.0;

  for (int attempt = 0; attempt < SPAWN_ATTEMPTS; attempt++) {
    double px = RandomDouble(currentProblem->lower, currentProblem->upper);
    double py = RandomDouble(currentProblem->lower, currentProblem->upper);
    double distance = NearestTerritoryDistanceSquared(px, py);

    if (distance > fallbackDistance) {
      fallbackDistance = distance;
      fallbackX = px;
      fallbackY = py;
    }

    if (distance >= territoryRadiusSquared) {
      *x = px;
      *y = py;
      return;
    }
  }

  // Only used if no permitted point was found in the attempt budget.
  *x = fallbackX;
  *y = fallbackY;
}

static double Evaluate(double x, double y, int anti) {
  double position[MAX_DIM] = {0.0};
  position[0] = x;
  position[1] = y;
  for (int d = 2; d < currentProblem->dim; d++)
    position[d] = RandomDouble(currentProblem->lower, currentProblem->upper);

  double value = currentProblem->function(position, currentProblem->dim);
  evaluations++;

  if (historyCount < MAX_EVALUATIONS) {
    history[historyCount].x = x;
    history[historyCount].y = y;
    historyCount++;
  }

  if (anti && territoryCount < MAX_EVALUATIONS) {
    territory[territoryCount].x = x;
    territory[territoryCount].y = y;
    territoryCount++;
  }

  if (value < bestValue) {
    bestValue = value;
    bestX = x;
    bestY = y;
  }
  return value;
}

static void RestartAgent(Agent *agent) {
  if (agent->anti) {
    agent->x = RandomDouble(currentProblem->lower, currentProblem->upper);
    agent->y = RandomDouble(currentProblem->lower, currentProblem->upper);
  } else
    ChooseNormalSpawn(&agent->x, &agent->y);

  agent->angle = RandomDouble(0.0, 2.0 * PI);
  agent->step = initialStep;
  agent->value = Evaluate(agent->x, agent->y, agent->anti);
}

static int Better(const Agent *agent, double candidate, double reference) {
  if (agent->anti)
    return candidate > reference;
  return candidate < reference;
}

static int TryDirection(Agent *agent, int direction, double *chosenX,
                        double *chosenY, double *chosenValue,
                        double *chosenAngle) {
  double angle =
      NormalizeAngle(agent->angle + RelativeAngles[direction] * PI / 180.0);
  double x = agent->x + agent->step * cos(angle);
  double y = agent->y + agent->step * sin(angle);

  if (x < currentProblem->lower || x > currentProblem->upper ||
      y < currentProblem->lower || y > currentProblem->upper)
    return 0;

  double value = Evaluate(x, y, agent->anti);
  if (!Better(agent, value, *chosenValue))
    return 0;

  *chosenX = x;
  *chosenY = y;
  *chosenValue = value;
  *chosenAngle = angle;
  return 1;
}

static void StepAgent(Agent *agent) {
  double x = agent->x;
  double y = agent->y;
  double value = agent->value;
  double angle = agent->angle;

  if (TryDirection(agent, 0, &x, &y, &value, &angle))
    goto Move;
  if (TryDirection(agent, 1, &x, &y, &value, &angle))
    goto Move;

  for (int i = 2; i < 4; i++)
    TryDirection(agent, i, &x, &y, &value, &angle);
  if (Better(agent, value, agent->value))
    goto Move;

  for (int i = 4; i < DIRECTION_COUNT; i++)
    TryDirection(agent, i, &x, &y, &value, &angle);
  if (Better(agent, value, agent->value))
    goto Move;

  agent->step *= 0.5;
  if (agent->step < minimumStep)
    RestartAgent(agent);
  return;

Move:
  agent->x = x;
  agent->y = y;
  agent->value = value;
  agent->angle = angle;
}

static void ScoutsDInit(const TestProblem *problem) {
  currentProblem = problem;
  initialStep = (problem->upper - problem->lower) * INITIAL_STEP_FRACTION;
  minimumStep = (problem->upper - problem->lower) * MIN_STEP_FRACTION;
  double radius = (problem->upper - problem->lower) * TERRITORY_RADIUS_FRACTION;
  territoryRadiusSquared = radius * radius;

  evaluations = 0;
  historyCount = 0;
  territoryCount = 0;
  nextAgent = 0;
  bestValue = DBL_MAX;
  bestX = 0.0;
  bestY = 0.0;

  for (int i = 0; i <= ANTI_AGENT_COUNT; i++) {
    agents[i].anti = (i != 0);
    RestartAgent(&agents[i]);
  }
}

static void ScoutsDStep(void) {
  StepAgent(&agents[nextAgent]);
  nextAgent = (nextAgent + 1) % (ANTI_AGENT_COUNT + 1);
}

static int ScoutsDGetPointCount(void) {
  return historyCount + ANTI_AGENT_COUNT + 1;
}

static void ScoutsDGetPoint(int index, double *x, double *y, int *type) {
  if (index < 0 || index >= ScoutsDGetPointCount())
    return;

  if (index < historyCount) {
    *x = history[index].x;
    *y = history[index].y;
    *type = 8;
    return;
  }

  Agent *agent = &agents[index - historyCount];
  *x = agent->x;
  *y = agent->y;
  *type = agent->anti ? 6 : 5;
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
