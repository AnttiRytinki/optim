#include <stdlib.h>

#include "optimizers.h"

extern InteractiveOptimizer ExploreExploitOptimizer;
extern InteractiveOptimizer RandomSearchOptimizer;
extern InteractiveOptimizer HillClimbOptimizer;
extern InteractiveOptimizer DifferentialEvolutionOptimizer;
extern InteractiveOptimizer ScoutHQOptimizer;
extern InteractiveOptimizer RandomHalvingOptimizer;
extern InteractiveOptimizer ScoutsAOptimizer;
extern InteractiveOptimizer ScoutsBOptimizer;
extern InteractiveOptimizer NelderMeadOptimizer;
extern InteractiveOptimizer ScoutsDOptimizer;
extern InteractiveOptimizer BasinHoppingOptimizer;
extern InteractiveOptimizer TabuSearchOptimizer;
extern InteractiveOptimizer ScoutsEOptimizer;
extern InteractiveOptimizer ScoutsFOptimizer;
extern InteractiveOptimizer ScoutsGOptimizer;

double RandomDouble(double min, double max) {
  return min + (max - min) * ((double)rand() / (double)RAND_MAX);
}

InteractiveOptimizer *Optimizers[] = {
    &HillClimbOptimizer,    &DifferentialEvolutionOptimizer,
    &ScoutHQOptimizer,      &RandomHalvingOptimizer,
    &ScoutsAOptimizer,      &ScoutsBOptimizer,
    &NelderMeadOptimizer,   &ScoutsDOptimizer,
    &BasinHoppingOptimizer, &TabuSearchOptimizer,
    &ScoutsEOptimizer,      &ScoutsFOptimizer,
    &ScoutsGOptimizer};

int OptimizerCount = sizeof(Optimizers) / sizeof(Optimizers[0]);
