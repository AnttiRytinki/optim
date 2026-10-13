
#!/bin/bash

printf '\e[8;48;160t'

# Compile benchmark
gcc -Wall -Wextra -O2 \
    opt_bench_interactive.c \
    optimizers.c \
    problems.c \
    HillClimbInteractive.c \
    DifferentialEvolutionInteractive.c \
    ScoutHQInteractive.c \
    RandomHalvingInteractive.c \
    ScoutsInteractiveA.c \
    ScoutsInteractiveB.c \
    ScoutsInteractiveD.c \
    NelderMeadInteractive.c \
    TabuSearchInteractive.c \
    BasinHoppingInteractive.c \
    ScoutsInteractiveE.c \
    ScoutsInteractiveF.c \
    ScoutsInteractiveG.c \
    ScoutsInteractiveH.c \
    -lm \
    -o opt_bench || exit 1

# Create timestamped results file
TIMESTAMP=$(date +"%Y-%m-%d_%H-%M-%S")

mkdir -p benchmark_results

RESULTS_FILE="benchmark_results/benchmark_results_$TIMESTAMP.txt"

# Run benchmark and save output
taskset -c 2 ./opt_bench "$@" | tee "$RESULTS_FILE"
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
