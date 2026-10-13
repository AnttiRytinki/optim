#ifndef DIRECTIONAL_TREE_INTERACTIVE_H
#define DIRECTIONAL_TREE_INTERACTIVE_H

#include "optimizers.h"

typedef struct {
  double minimumStep;
  int refinementCount;
  int coverageComplete;
} ScoutHQDiagnostics;

extern InteractiveOptimizer ScoutHQOptimizer;

void ScoutHQGetDiagnostics(ScoutHQDiagnostics *diagnostics);

#endif
