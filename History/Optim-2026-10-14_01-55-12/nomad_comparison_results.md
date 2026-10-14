# ScoutsD versus NOMAD 4.6.0

200 paired trials per algorithm per problem; base seed **20261014**. Each trial uses identical starting coordinates, objective and bounds. Success is `abs(best - optimumValue) < 1e-6`; initialization counts toward the 100,000-call budget. All 16,800 checkpoint rows passed count, pairing, seed, finite-value and monotonicity checks.

## Final results at 100,000 evaluations

| Problem | Algorithm | Mean best | Median best | Success | Wilson 95% CI | Median first hit, successful runs | Actual calls min / mean / max |
|---|---|---:|---:|---:|---:|---:|---|
| Sphere | ScoutsD | 1.6882e-15 | 1.1435e-15 | 200/200 (100.0%) | 98.1–100.0% | 104 | 100000 / 100003.0 / 100007 |
| Sphere | NOMAD | 5.0968e-18 | 1.6988e-21 | 200/200 (100.0%) | 98.1–100.0% | 60 | 145 / 252.2 / 670 |
| Rastrigin | ScoutsD | 1.2633e-11 | 8.7415e-12 | 200/200 (100.0%) | 98.1–100.0% | 1339 | 100000 / 100003.4 / 100039 |
| Rastrigin | NOMAD | 0.33331 | 0 | 190/200 (95.0%) | 91.0–97.3% | 81.5 | 159 / 219.4 / 302 |
| Rosenbrock | ScoutsD | 8.5952e-11 | 1.6824e-11 | 200/200 (100.0%) | 98.1–100.0% | 4163 | 100000 / 100002.8 / 100007 |
| Rosenbrock | NOMAD | 3.5308e-16 | 1.8686e-20 | 200/200 (100.0%) | 98.1–100.0% | 160 | 231 / 421.0 / 1172 |
| Ackley | ScoutsD | 4.4575e-07 | 4.2554e-07 | 199/200 (99.5%) | 97.2–99.9% | 3965 | 100000 / 100003.7 / 100043 |
| Ackley | NOMAD | 0.14852 | 1.5321e-13 | 196/200 (98.0%) | 95.0–99.2% | 187 | 199 / 441.5 / 506 |
| Himmelblau | ScoutsD | 5.0816e-14 | 3.2658e-14 | 200/200 (100.0%) | 98.1–100.0% | 135 | 100000 / 100003.0 / 100007 |
| Himmelblau | NOMAD | 2.3506e-18 | 1.8026e-21 | 200/200 (100.0%) | 98.1–100.0% | 83.5 | 157 / 271.0 / 515 |
| Griewank | ScoutsD | 0.00085055 | 1.0297e-09 | 177/200 (88.5%) | 83.3–92.2% | 34096 | 100000 / 100003.9 / 100086 |
| Griewank | NOMAD | 0.091838 | 0.059178 | 4/200 (2.0%) | 0.8–5.0% | 120.5 | 204 / 279.5 / 421 |

## Mean best objective by exact evaluation checkpoint

| Problem | Algorithm | 100 | 300 | 1000 | 3000 | 10000 | 30000 | 100000 |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| Sphere | ScoutsD | 6.4898e-06 | 3.477e-13 | 1.6186e-13 | 6.2892e-14 | 1.7999e-14 | 6.3851e-15 | 1.6882e-15 |
| Sphere | NOMAD | 0.0054409 | 5.1056e-18 | 5.0968e-18 | 5.0968e-18 | 5.0968e-18 | 5.0968e-18 | 5.0968e-18 |
| Rastrigin | ScoutsD | 16.412 | 10.855 | 2.9446 | 5.4222e-11 | 3.6789e-11 | 2.9287e-11 | 1.2633e-11 |
| Rastrigin | NOMAD | 0.34414 | 0.33331 | 0.33331 | 0.33331 | 0.33331 | 0.33331 | 0.33331 |
| Rosenbrock | ScoutsD | 0.59188 | 0.22284 | 0.009757 | 0.0011669 | 7.1352e-07 | 6.7587e-10 | 8.5952e-11 |
| Rosenbrock | NOMAD | 0.056237 | 8.9681e-13 | 3.5308e-16 | 3.5308e-16 | 3.5308e-16 | 3.5308e-16 | 3.5308e-16 |
| Ackley | ScoutsD | 7.8731 | 6.1005 | 1.969 | 1.1729e-06 | 8.5472e-07 | 6.6588e-07 | 4.4575e-07 |
| Ackley | NOMAD | 0.15017 | 0.14852 | 0.14852 | 0.14852 | 0.14852 | 0.14852 | 0.14852 |
| Himmelblau | ScoutsD | 0.00061101 | 1.4159e-11 | 5.9187e-12 | 1.7995e-12 | 5.114e-13 | 1.766e-13 | 5.0816e-14 |
| Himmelblau | NOMAD | 1.9629e-05 | 2.3541e-18 | 2.3506e-18 | 2.3506e-18 | 2.3506e-18 | 2.3506e-18 | 2.3506e-18 |
| Griewank | ScoutsD | 0.46062 | 0.36608 | 0.1094 | 0.041388 | 0.013004 | 0.004746 | 0.00085055 |
| Griewank | NOMAD | 0.10436 | 0.091838 | 0.091838 | 0.091838 | 0.091838 | 0.091838 | 0.091838 |

## Success percentage by checkpoint

| Problem | Algorithm | 100 | 300 | 1000 | 3000 | 10000 | 30000 | 100000 |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| Sphere | ScoutsD | 37.0% | 100.0% | 100.0% | 100.0% | 100.0% | 100.0% | 100.0% |
| Sphere | NOMAD | 98.0% | 100.0% | 100.0% | 100.0% | 100.0% | 100.0% | 100.0% |
| Rastrigin | ScoutsD | 0.0% | 1.0% | 28.0% | 100.0% | 100.0% | 100.0% | 100.0% |
| Rastrigin | NOMAD | 76.5% | 95.0% | 95.0% | 95.0% | 95.0% | 95.0% | 95.0% |
| Rosenbrock | ScoutsD | 0.5% | 1.0% | 16.0% | 39.5% | 97.0% | 100.0% | 100.0% |
| Rosenbrock | NOMAD | 6.0% | 100.0% | 100.0% | 100.0% | 100.0% | 100.0% | 100.0% |
| Ackley | ScoutsD | 0.0% | 0.5% | 10.0% | 38.0% | 69.0% | 85.0% | 99.5% |
| Ackley | NOMAD | 3.5% | 98.0% | 98.0% | 98.0% | 98.0% | 98.0% | 98.0% |
| Himmelblau | ScoutsD | 3.5% | 100.0% | 100.0% | 100.0% | 100.0% | 100.0% | 100.0% |
| Himmelblau | NOMAD | 81.5% | 100.0% | 100.0% | 100.0% | 100.0% | 100.0% | 100.0% |
| Griewank | ScoutsD | 0.0% | 0.0% | 0.5% | 4.0% | 17.0% | 39.5% | 88.5% |
| Griewank | NOMAD | 0.0% | 2.0% | 2.0% | 2.0% | 2.0% | 2.0% | 2.0% |

## Paired final success outcomes

| Problem | Both succeed | ScoutsD only | NOMAD only | Neither |
|---|---:|---:|---:|---:|
| Sphere | 200 | 0 | 0 | 0 |
| Rastrigin | 190 | 10 | 0 | 0 |
| Rosenbrock | 200 | 0 | 0 | 0 |
| Ackley | 195 | 4 | 1 | 0 |
| Himmelblau | 200 | 0 | 0 | 0 |
| Griewank | 2 | 175 | 2 | 21 |

## Method and limitations

- NOMAD uses its standard serial MADS interface and default searches: quadratic model search and Nelder–Mead search enabled, default `ORTHO N+1 QUAD` poll directions. Single start, no added global restarts. Cache hits and model evaluations do not count as objective calls. State is reset between runs.
- ScoutsD source is unchanged. A recording objective wrapper samples exact checkpoints, including evaluations in Init. Whole Steps may overshoot the final budget; excess calls are counted above but excluded from the reported 100,000-call result.
- NOMAD may terminate naturally before the budget. Its final best is held at later checkpoints, without restarting it or pretending to spend unused evaluations.
- The 200 trials were split into four independent processes with 50 trials each, offsets 0, 50, 100, 150. These are evaluation-count results, not timing measurements. The deterministic seed schedule covers precisely runs 1–200 in every problem/algorithm pair.
- First-hit medians are conditional on success, so a low median does not compensate for a poor success rate. Confidence intervals summarize variability across these seeded starts; they do not generalize to arbitrary problems.
- All problems are deterministic and two-dimensional. Comparing default single-start NOMAD with ScoutsD, which includes restarts and ridge searches, compares these configurations, not every possible NOMAD configuration. No post hoc parameter tuning was performed.
- A single fixed base seed and libc rand are used for starting-point generation. Repeatability was verified on this build/platform; cross-platform bitwise RNG/floating-point reproducibility is not guaranteed.
- Build: gcc/g++ 13.3.0, benchmark and adapter `-O2`, existing NOMAD 4.6.0 Release shared libraries. Executables and intermediate build artifacts live in `/tmp`. `build.sh` was not invoked. Nothing was committed or pushed.

## Reproduction

```bash
NOMAD_EXE=$(bash nomad_benchmark_build.sh)
"$NOMAD_EXE" -optlist nomad_benchmark_optlist.txt -seed 20261014 -runs 200 -csv /tmp/nomad_results.csv
```

The result values can be reproduced in a single process using this command; the four-process partition only changed scheduling. Timing uses a separate serial run and is machine/load dependent.

CSV SHA-256: `2dcc9675247ce2a737ba3d29ea7fcc34cebe0e98ac088898ce308470af03544e`.

## Separate serial timing diagnostic

20 paired trials per problem (first 20 seeds of the same schedule), 300 ms window. These exploratory timings include initialization and recording overhead and are not a controlled 100-run timing study. No concurrent comparison batches were running. System load and CPU frequency were not controlled; brief compilation work also occurred during this diagnostic. Success and throughput include only objective calls completed within 300 ms. NOMAD continued internally beyond the window where necessary; results were selected from its actual timestamps.

| Problem | Algorithm | Success by 300 ms | Median first hit among successes (ms) | Evaluations/s in window |
|---|---|---:|---:|---:|
| Sphere | ScoutsD | 100.0% | 0.007660 | 15,467,845 |
| Sphere | NOMAD | 100.0% | 233.108099 | 315 |
| Rastrigin | ScoutsD | 100.0% | 0.115154 | 11,600,132 |
| Rastrigin | NOMAD | 45.0% | 251.465715 | 272 |
| Rosenbrock | ScoutsD | 100.0% | 0.257388 | 15,733,884 |
| Rosenbrock | NOMAD | 5.0% | 262.828895 | 328 |
| Ackley | ScoutsD | 100.0% | 0.340970 | 10,244,146 |
| Ackley | NOMAD | 5.0% | 101.908846 | 293 |
| Himmelblau | ScoutsD | 100.0% | 0.010130 | 15,514,419 |
| Himmelblau | NOMAD | 35.0% | 278.153071 | 256 |
| Griewank | ScoutsD | 100.0% | 3.573464 | 9,949,820 |
| Griewank | NOMAD | 0.0% | N/A | 231 |

At very short deadlines NOMAD sometimes had no completed evaluation, reported as infinity in the raw tables. ScoutsD’s much higher throughput on these cheap objectives shows why fewer function evaluations need not mean less wall-clock time. Timing does not include CSV formatting, reporting, or post-run history replay.

```bash
"$NOMAD_EXE" -optlist nomad_benchmark_optlist.txt -seed 20261014 -runs 1 -t -time-runs 20
```

The preliminary one-run evaluation section from this command is not part of the 200-run study. Raw timing output: `nomad_comparison_timing.txt`.

## Validation performed

- Adapter C API tests covered all six objectives, budgets 1/100/100000, independent objective-call counts, running-best coordinates, budget/time queries, invalid inputs, and repeat independence.
- Benchmark instrumentation tests verified exact checkpoint sampling, exclusion of future values at time deadlines, and an unchanged ScoutsD trajectory.
- Separate-process smoke CSVs matched byte-for-byte; their first two trials also matched the full experiment. The plain gcc build produced identical ScoutsD results to the optional NOMAD build.
- Full-run logs contained no runtime warnings or failures. Shared changes are confined to optimizers.c and opt_bench_interactive.c; ScoutsD, problems, build.sh, and the standalone Sphere test were preserved.
