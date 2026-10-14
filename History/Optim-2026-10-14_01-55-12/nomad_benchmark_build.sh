#!/bin/bash
# Optional NOMAD build. All generated files stay in /tmp. No Git operations.
set -euo pipefail
cd -- "$(dirname -- "$0")"
NOMAD_BUILD_DIR=$(mktemp -d /tmp/optim-nomad-build.XXXXXX)
SOURCES=(opt_bench_interactive.c optimizers.c problems.c
    HillClimbInteractive.c DifferentialEvolutionInteractive.c
    ScoutHQInteractive.c RandomHalvingInteractive.c
    ScoutsInteractiveA.c ScoutsInteractiveB.c ScoutsInteractiveD.c
    NelderMeadInteractive.c TabuSearchInteractive.c BasinHoppingInteractive.c
    ScoutsInteractiveE.c ScoutsInteractiveF.c ScoutsInteractiveG.c ScoutsInteractiveH.c)
OBJECTS=()
for SOURCE in "${SOURCES[@]}"
do
    OBJECT="$NOMAD_BUILD_DIR/${SOURCE%.c}.o"
    gcc -Wall -Wextra -O2 -DOPTIM_WITH_NOMAD -c "$SOURCE" -o "$OBJECT"
    OBJECTS+=("$OBJECT")
done
g++ -std=c++17 -O2 -isystem External/nomad/src nomad_adapter.cpp \
    "${OBJECTS[@]}" -LExternal/nomad/build/src \
    -Wl,-rpath,"$PWD/External/nomad/build/src" \
    -lnomadAlgos -lnomadEval -lnomadUtils -lm \
    -o "$NOMAD_BUILD_DIR/opt_bench_nomad"
printf '%s\n' "$NOMAD_BUILD_DIR/opt_bench_nomad"
