#!/bin/bash

gcc -Wall -Wextra -O2 \
    opt_view.c \
    optimizers.c \
    ExploreExploitInteractive.c \
    RandomSearchInteractive.c \
    HillClimbInteractive.c \
    DifferentialEvolutionInteractive.c \
    ScoutHQInteractive.c \
    problems.c \
    -lSDL2 -lm \
    -o opt_view || exit 1

./opt_view

TIMESTAMP=$(date +"%Y-%m-%d_%H-%M-%S")
DESTINATION="History/Optim-$TIMESTAMP"

mkdir -p "$DESTINATION"

find . -maxdepth 1 -type f -exec cp {} "$DESTINATION" \;

git add .

if ! git diff --cached --quiet
then
    git commit -m "Update $TIMESTAMP"
    git push
fi
