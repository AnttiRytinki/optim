# NOMAD benchmark adapter

`nomad_adapter.h` is a C API backed by standard NOMAD 4.6 MADS
(`MainStep::start/run/end`). `nomad_sphere_test.cpp` is preserved.

The caller provides the existing `TestProblem`, starting coordinates, a
nonnegative NOMAD seed, and the maximum actual black-box evaluation budget.
The adapter uses NOMAD's default searches and stopping criteria, with one
serial evaluation thread and no output files. It does not restart MADS when
it terminates early. Cache, RNG, evaluator control, tags, and parameter state
are reset for each run, including cleanup on errors. Concurrent runs in one
process are unsupported.

Every evaluator callback calls the supplied objective exactly once and stores
its coordinates, objective, running best coordinates/value, and monotonic
elapsed time. NOMAD's final BB count must equal the history length and must
not exceed the budget. Cache hits and internal model calculations never call
this evaluator and never add records. Checkpoint queries use evaluation N
exactly, or the final record if N exceeds the actual stopping count. Actual
counts are always separate from requested budgets.

Time records begin before the initial reset and parameter setup, include the
starting evaluation, and timestamp objective completion. Total elapsed time
includes MADS termination, but excludes destruction and the final cleanup
reset. Post-run history queries are not optimization runtime. A fair time
comparison must use these timestamps, not time replaying the history. Runs
should be capped by time for a dedicated time benchmark; the current adapter
only supports a black-box evaluation budget.

For a paired comparison without modifying ScoutsD, reseed C rand separately
for each algorithm/run. Generate NOMAD's starting point from the first two
RandomDouble-equivalent draws, matching ScoutsD's initial coordinates.
ScoutsD's angle consumes its third draw; NOMAD uses its own seeded RNG for
subsequent directions. This pairs starts, not subsequent random draws.
Use the same objective, bounds, 100000-evaluation budget, and success condition
`fabs(best - optimumValue) < 1e-6`. Keep all default NOMAD searches documented;
this is a comparison with default single-start MADS, whereas ScoutsD includes
restarts and ridge searches. Additional NOMAD configurations require separate
labels and experiments rather than tuning on reported test runs.

The shared runner currently samples after Steps (ScoutsD may overshoot exact
checkpoints), uses a clock seed, assumes counts never stop growing, and excludes
Init from timing. For exact ScoutsD histories, the runner can pass a copied
TestProblem whose objective callback records actual evaluations and best-so-far
values, with recording established before Init. This changes instrumentation,
not ScoutsD. Use the callback history for exact budget results; discard entries
beyond 100000 if the final Step overshoots and report that overshoot explicitly.

Adapter test: compile problems.c and nomad_adapter_test.c with gcc, then link
those objects and nomad_adapter.cpp with g++ and the existing nomadAlgos,
nomadEval, and nomadUtils shared libraries. Write all objects, executables, and
logs to /tmp. Do not use build.sh for this test: it also commits and pushes.

## Integrated benchmark

The optional build is `bash nomad_benchmark_build.sh`. It prints the executable
path under `/tmp`; it defines `OPTIM_WITH_NOMAD` to register the batch-only NOMAD
entry. A direct gcc-only build remains supported without NOMAD. `build.sh` now calls
this helper, so `./build.sh -t -optlist optlist.txt | outclip` includes NOMAD
when selected. Its existing result saving, history backup, and Git commit/push
steps are preserved. Automated validation invokes the build helper and benchmark
directly, without executing those Git steps.

Run the resulting executable with `-optlist nomad_benchmark_optlist.txt`.
Defaults: 200 evaluation runs, base seed 20261014, 100 timed runs with `-t`.
Options: `-seed N`, `-runs N`, `-run-offset N`, `-time-runs N`, and
`-csv /tmp/results.csv`. The CSV stores each run/checkpoint's value, true
objective-call stopping count, paired starting coordinates, and first successful
evaluation (0 means no success by the budget).

Per-run seed is `(baseSeed + 1000003 * problemIndex + 7919 * runIndex) %
2147483647`, with zero-based problem/run indices. A run offset allows independent
processes to execute disjoint portions of the same reproducible experiment.
Seeds are reset before each algorithm rather than shared as a progressing stream.

The runner passes a copied TestProblem with a recording objective to all C
optimizers. It records exactly the 100th, 300th, etc. completed objective call.
ScoutsD remains unchanged. A Step may run past the final budget; excess calls
are counted and reported but do not affect checkpoint results or success times.
The recorder consumes no random numbers. NOMAD checkpoint values come from its
actual evaluation history, with final values carried forward after early stopping.

Timing includes initialization, parameter setup, objective calls, and search/model
work. Each deadline uses the best of evaluations completed on or before it;
no completed evaluation means infinity. NOMAD's timed run still executes to the
100000 evaluation cap or natural termination, then its recorded timestamps select
the first 300 ms. This is not a deadline-stopped optimizer implementation.
ScoutsD likewise may finish a Step after the deadline; later evaluations are
excluded. Throughput counts only evaluations completed within 300 ms, divided
by that fixed window. Timings include recording overhead and are machine/load
dependent. Do not run time comparisons concurrently with other benchmark batches.

The six objectives all have optimumValue 0; the success rule is unchanged:
`fabs(best - optimumValue) < 1e-6`. NOMAD uses default single-start MADS searches,
including quadratic models; ScoutsD uses its existing restart/ridge mechanisms.
This configuration comparison does not establish which method is best in other
dimensions, domains, constraints, noisy objectives, or alternative NOMAD settings.

`-csv` records evaluation-checkpoint data only; `-t` writes timing tables to
standard output. The saved 200-run experiment is in
`nomad_comparison_results.csv` and `nomad_comparison_results.md`.
