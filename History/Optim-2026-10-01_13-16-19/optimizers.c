#include <stdlib.h>

#include "optimizers.h"

extern InteractiveOptimizer ExploreExploitOptimizer;
extern InteractiveOptimizer RandomSearchOptimizer;
extern InteractiveOptimizer HillClimbOptimizer;
extern InteractiveOptimizer DifferentialEvolutionOptimizer;
extern InteractiveOptimizer ScoutHQOptimizer;

double RandomDouble(double min, double max) {
  return min + (max - min) * ((double)rand() / (double)RAND_MAX);
}

InteractiveOptimizer *Optimizers[] = {
    &HillClimbOptimizer, &DifferentialEvolutionOptimizer, &ScoutHQOptimizer};

int OptimizerCount = sizeof(Optimizers) / sizeof(Optimizers[0]);
