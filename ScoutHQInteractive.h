#ifndef DIRECTIONAL_TREE_INTERACTIVE_H
#define DIRECTIONAL_TREE_INTERACTIVE_H

#include "optimizers.h"

typedef struct {
  double closestOrigin;
  int closestOriginEvaluation;
  double closestOriginStepSize;
  double bestValue;
  int teleportCount;
  int refinementCount;
} ScoutHQAgentDiagnostics;

typedef struct {
  double minimumStep;
  int refinementCount;
  int coverageComplete;

  ScoutHQAgentDiagnostics agents[8];
} ScoutHQDiagnostics;

extern InteractiveOptimizer ScoutHQOptimizer;

void ScoutHQGetDiagnostics(ScoutHQDiagnostics *diagnostics);

#endif
