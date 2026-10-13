#include <float.h>
#include <stdlib.h>

#include "RandomHalvingInteractive.h"
#include "optimizers.h"

#define GRID_SIZE 16
#define CELL_COUNT (GRID_SIZE * GRID_SIZE)
#define MIN_SAMPLES_PER_CELL 4
#define MAX_POINTS 100000

#define RANDOM_HALVING_ACTIVE_TYPE 9
#define RANDOM_HALVING_INACTIVE_TYPE 10
#define RANDOM_HALVING_TESTED_TYPE 11

typedef struct {
  double lowerX;
  double upperX;
  double lowerY;
  double upperY;

  double bestValue;

  int active;
  int roundSampleCount;
} Cell;

typedef struct {
  double x;
  double y;
  double value;
  int active;
} SamplePoint;

static const TestProblem *currentProblem;

static Cell cells[CELL_COUNT];

static SamplePoint points[MAX_POINTS];
static int pointCount;

static double regionLowerX;
static double regionUpperX;
static double regionLowerY;
static double regionUpperY;

static int activeCellCount;

static double bestX;
static double bestY;
static double bestValue;

static int evaluations;

static void CreateGrid(double lowerX, double upperX, double lowerY,
                       double upperY) {
  double cellWidth = (upperX - lowerX) / GRID_SIZE;
  double cellHeight = (upperY - lowerY) / GRID_SIZE;

  for (int y = 0; y < GRID_SIZE; y++) {
    for (int x = 0; x < GRID_SIZE; x++) {
      int index = y * GRID_SIZE + x;
      Cell *cell = &cells[index];

      cell->lowerX = lowerX + x * cellWidth;
      cell->upperX = cell->lowerX + cellWidth;

      cell->lowerY = lowerY + y * cellHeight;
      cell->upperY = cell->lowerY + cellHeight;

      cell->bestValue = DBL_MAX;

      cell->active = 1;
      cell->roundSampleCount = 0;
    }
  }

  activeCellCount = CELL_COUNT;
}

static int GetRandomActiveCell(void) {
  int target = rand() % activeCellCount;
  int activeIndex = 0;

  for (int i = 0; i < CELL_COUNT; i++) {
    if (!cells[i].active)
      continue;

    if (activeIndex == target)
      return i;

    activeIndex++;
  }

  return -1;
}

static int AllActiveCellsSampled(void) {
  for (int i = 0; i < CELL_COUNT; i++) {
    if (!cells[i].active)
      continue;

    if (cells[i].roundSampleCount < MIN_SAMPLES_PER_CELL)
      return 0;
  }

  return 1;
}

static int FindWorstActiveCell(void) {
  int worstIndex = -1;
  double worstValue = -DBL_MAX;

  for (int i = 0; i < CELL_COUNT; i++) {
    if (!cells[i].active)
      continue;

    if (cells[i].bestValue > worstValue) {
      worstValue = cells[i].bestValue;
      worstIndex = i;
    }
  }

  return worstIndex;
}

static void ResetRoundSampleCounts(void) {
  for (int i = 0; i < CELL_COUNT; i++) {
    if (cells[i].active)
      cells[i].roundSampleCount = 0;
  }
}

static void ZoomIntoLastCell(void) {
  int winner = -1;

  for (int i = 0; i < CELL_COUNT; i++) {
    if (cells[i].active) {
      winner = i;
      break;
    }
  }

  if (winner < 0)
    return;

  regionLowerX = cells[winner].lowerX;
  regionUpperX = cells[winner].upperX;
  regionLowerY = cells[winner].lowerY;
  regionUpperY = cells[winner].upperY;

  CreateGrid(regionLowerX, regionUpperX, regionLowerY, regionUpperY);
}

static void HalveCells(void) {
  int cellsToRemove = activeCellCount / 2;

  for (int i = 0; i < cellsToRemove; i++) {
    int worst = FindWorstActiveCell();

    if (worst < 0)
      break;

    cells[worst].active = 0;
    activeCellCount--;
  }

  if (activeCellCount == 1) {
    ZoomIntoLastCell();
    return;
  }

  ResetRoundSampleCounts();
}

static void EvaluateRandomPoint(void) {
  int cellIndex = GetRandomActiveCell();

  if (cellIndex < 0)
    return;

  Cell *cell = &cells[cellIndex];

  double x = RandomDouble(cell->lowerX, cell->upperX);
  double y = RandomDouble(cell->lowerY, cell->upperY);

  double position[MAX_DIM] = {0};

  position[0] = x;
  position[1] = y;

  for (int d = 2; d < currentProblem->dim; d++) {
    position[d] = RandomDouble(currentProblem->lower, currentProblem->upper);
  }

  double value = currentProblem->function(position, currentProblem->dim);

  evaluations++;

  cell->roundSampleCount++;

  if (value < cell->bestValue)
    cell->bestValue = value;

  if (value < bestValue) {
    bestValue = value;
    bestX = x;
    bestY = y;
  }

  if (pointCount < MAX_POINTS) {
    points[pointCount].x = x;
    points[pointCount].y = y;
    points[pointCount].value = value;
    points[pointCount].active = cell->active;

    pointCount++;
  }
}

static void RandomHalvingInit(const TestProblem *problem) {
  currentProblem = problem;

  pointCount = 0;
  evaluations = 0;

  bestX = 0.0;
  bestY = 0.0;
  bestValue = DBL_MAX;

  regionLowerX = problem->lower;
  regionUpperX = problem->upper;
  regionLowerY = problem->lower;
  regionUpperY = problem->upper;

  CreateGrid(regionLowerX, regionUpperX, regionLowerY, regionUpperY);
}

static void RandomHalvingStep(void) {
  EvaluateRandomPoint();

  if (AllActiveCellsSampled())
    HalveCells();
}

static int RandomHalvingGetPointCount(void) {
  return pointCount + activeCellCount;
}

static void RandomHalvingGetPoint(int index, double *x, double *y, int *type) {
  if (index < pointCount) {
    *x = points[index].x;
    *y = points[index].y;
    *type = RANDOM_HALVING_TESTED_TYPE;
    return;
  }

  int target = index - pointCount;
  int activeIndex = 0;

  for (int i = 0; i < CELL_COUNT; i++) {
    if (!cells[i].active)
      continue;

    if (activeIndex == target) {
      *x = (cells[i].lowerX + cells[i].upperX) * 0.5;
      *y = (cells[i].lowerY + cells[i].upperY) * 0.5;
      *type = RANDOM_HALVING_ACTIVE_TYPE;
      return;
    }

    activeIndex++;
  }

  *x = 0.0;
  *y = 0.0;
  *type = RANDOM_HALVING_INACTIVE_TYPE;
}

static void RandomHalvingGetBest(double *x, double *y, double *value,
                                 int *evaluationCount) {
  *x = bestX;
  *y = bestY;
  *value = bestValue;
  *evaluationCount = evaluations;
}

InteractiveOptimizer RandomHalvingOptimizer = {
    "RandomHalving",       RandomHalvingInit,
    RandomHalvingStep,     RandomHalvingGetPointCount,
    RandomHalvingGetPoint, RandomHalvingGetBest};
