#!/bin/bash

VIEW_EXECUTABLE=$(bash ./nomad_benchmark_build.sh --view) || exit 1
"$VIEW_EXECUTABLE" "$@" || exit 1

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
