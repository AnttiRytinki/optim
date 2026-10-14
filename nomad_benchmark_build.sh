#!/bin/bash
# NOMAD benchmark or viewer build. All generated files stay in /tmp. No Git operations.
set -euo pipefail
cd -- "$(dirname -- "$0")"
NOMAD_BUILD_DIR=$(mktemp -d /tmp/optim-nomad-build.XXXXXX)
MAIN_SOURCE=opt_bench_interactive.c
OUTPUT_NAME=opt_bench_nomad
EXTRA_FLAGS=()
EXTRA_LIBS=()
EXTRA_SOURCES=()
if [[ $# -eq 1 && "$1" == "--view" ]]
then
    MAIN_SOURCE=opt_view.c
    OUTPUT_NAME=opt_view
    EXTRA_FLAGS=(-DOPTIM_NOMAD_VIEW)
    EXTRA_LIBS=(-lSDL2)
    EXTRA_SOURCES=(nomad_view.c)
elif [[ $# -ne 0 ]]
then
    echo "Usage: bash nomad_benchmark_build.sh [--view]" >&2
    exit 1
fi
SOURCES=("$MAIN_SOURCE" "${EXTRA_SOURCES[@]}" optimizers.c problems.c
    HillClimbInteractive.c DifferentialEvolutionInteractive.c
    ScoutHQInteractive.c RandomHalvingInteractive.c
    ScoutsInteractiveA.c ScoutsInteractiveB.c ScoutsInteractiveD.c
    NelderMeadInteractive.c TabuSearchInteractive.c BasinHoppingInteractive.c
    ScoutsInteractiveE.c ScoutsInteractiveF.c ScoutsInteractiveG.c ScoutsInteractiveH.c)
OBJECTS=()
for SOURCE in "${SOURCES[@]}"
do
    OBJECT="$NOMAD_BUILD_DIR/${SOURCE%.c}.o"
    gcc -Wall -Wextra -O2 -DOPTIM_WITH_NOMAD "${EXTRA_FLAGS[@]}" -c "$SOURCE" -o "$OBJECT"
    OBJECTS+=("$OBJECT")
done
g++ -std=c++17 -O2 -isystem External/nomad/src nomad_adapter.cpp \
    "${OBJECTS[@]}" -LExternal/nomad/build/src \
    -Wl,-rpath,"$PWD/External/nomad/build/src" \
    -lnomadAlgos -lnomadEval -lnomadUtils "${EXTRA_LIBS[@]}" -lm \
    -o "$NOMAD_BUILD_DIR/$OUTPUT_NAME"
printf '%s\n' "$NOMAD_BUILD_DIR/$OUTPUT_NAME"
