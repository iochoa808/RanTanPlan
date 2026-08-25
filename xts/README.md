# PDDL-XTS — the extended-syntax extension

Everything in this directory is the PDDL-XTS extension: bounded integers, arrays,
sets and the count operator, layered on top of standard PDDL. The base RanTanPlan
project is untouched by it and lives at the repo root (`rantanplan/`, `build.py`,
`test.py`, `scripts/`, `pddl/`).

Both halves share one C++ backend and one installed package, so everything here
runs from the **repo root**, not from this directory:

```bash
source .venv/bin/activate
python xts/tools/solve.py -d domain.pddl -p problem.pddl
```

## Layout

```
tools/            solve.py     one problem, with --trace into every pipeline layer
                  bench.py     performance sweeps (compile axis × runtime axis)
                  compare.py   does RTP agree with UP / with an external solver / with itself
                  visualize_pipeline.py   UP-compiler × XTS-feature compatibility matrix
                  test_PDDL-XTS.py  the correctness matrix
                                    (source × pipeline × solver × encoding × frame mode)

benchmarks/       unit/          145 fixtures: 95 expected to solve, 49 X_* break
                                 targets expected to be rejected or proven
                                 unsolvable, 1 Python-only
                  domains/       11 hand-written domains with PDDL instances
                  translations/  48 PDDL-XTS problems translated to plain PDDL,
                                 for solvers that can't read XTS
                  viewpoints/    12 domains, each modelled several ways, to compare
                                 encodings
                  generated/     instance generators and the instances they produced
                  scaling/       80 size-parametrized instances, 11 domains, five
                                 of them classical-vs-XTS pairs

cluster/          SLURM sweeps. submit.sh queues an array plus a merge job that
                  waits on it, so results land merged. env.sh holds the shared
                  module/venv setup and run_sweep(), which every *.sbatch calls.

results/          Benchmark output: one directory per sweep per run, holding
                  summary.csv, raw.csv, run.json, shards/ and logs/ (gitignored).
```

Benchmark output — merged CSVs, shards, manifests and SLURM logs — all lands
in `xts/results/<experiment>/<run>/`, which is gitignored.

## Quick check that things work

```bash
python xts/tools/test_PDDL-XTS.py --filter 2d --pipeline native   # a few array fixtures
python xts/tools/bench.py --families xts-unit --list   # what the suite would run
```