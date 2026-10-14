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
extern InteractiveOptimizer ScoutsHOptimizer;

double RandomDouble(double min, double max) {
  return min + (max - min) * ((double)rand() / (double)RAND_MAX);
}

#ifdef OPTIM_WITH_NOMAD
/* Batch-only entry: the benchmark dispatches through nomad_adapter.h. */
InteractiveOptimizer NomadOptimizer = {"NOMAD", NULL, NULL, NULL, NULL, NULL};
#endif

InteractiveOptimizer *Optimizers[] = {
#ifdef OPTIM_WITH_NOMAD
    &NomadOptimizer,
#endif
    &HillClimbOptimizer,    &DifferentialEvolutionOptimizer,
    &ScoutHQOptimizer,      &RandomHalvingOptimizer,
    &ScoutsAOptimizer,      &ScoutsBOptimizer,
    &NelderMeadOptimizer,   &ScoutsDOptimizer,
    &BasinHoppingOptimizer, &TabuSearchOptimizer,
    &ScoutsEOptimizer,      &ScoutsFOptimizer,
    &ScoutsGOptimizer,      &ScoutsHOptimizer};

int OptimizerCount = sizeof(Optimizers) / sizeof(Optimizers[0]);
