#!/bin/bash

printf '\e[8;48;160t'

# Compile C optimizers and the C++ NOMAD adapter into /tmp.
BENCHMARK_EXECUTABLE=$(bash ./nomad_benchmark_build.sh) || exit 1

# Create timestamped results file
TIMESTAMP=$(date +"%Y-%m-%d_%H-%M-%S")

mkdir -p benchmark_results

RESULTS_FILE="benchmark_results/benchmark_results_$TIMESTAMP.txt"

# Run benchmark and save output
taskset -c 2 "$BENCHMARK_EXECUTABLE" "$@" | tee "$RESULTS_FILE"
BENCHMARK_STATUS=${PIPESTATUS[0]}

if [ "$BENCHMARK_STATUS" -ne 0 ]
then
    echo "Benchmark failed with exit code $BENCHMARK_STATUS" >&2
    exit "$BENCHMARK_STATUS"
fi

# Create history backup
DESTINATION="History/Optim-$TIMESTAMP"

mkdir -p "$DESTINATION"

find . -maxdepth 1 -type f -exec cp {} "$DESTINATION" \;

# Commit everything, including benchmark results
git add .

if ! git diff --cached --quiet
then
    git commit -m "Benchmark update $TIMESTAMP" || exit 1
    git push || exit 1
fi
