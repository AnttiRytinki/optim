#!/bin/bash

printf '\e[8;48;160t'

gcc -Wall -Wextra -O2 \
    opt_bench_interactive.c \
    optimizers.c \
    problems.c \
    ExploreExploitInteractive.c \
    RandomSearchInteractive.c \
    HillClimbInteractive.c \
    DifferentialEvolutionInteractive.c \
    DirectionalTreeInteractive.c \
    -lm \
    -o opt_bench || exit 1

./opt_bench

TIMESTAMP=$(date +"%Y-%m-%d_%H-%M-%S")
DESTINATION="History/Optim-$TIMESTAMP"

mkdir -p "$DESTINATION"

find . -maxdepth 1 -type f -exec cp {} "$DESTINATION" \;

git add .

if ! git diff --cached --quiet
then
    git commit -m "Benchmark update $TIMESTAMP"
    git push
fi
