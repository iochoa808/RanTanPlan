#!/bin/bash
#SBATCH --partition=normal
#SBATCH --job-name=rtp-03_dump_trucks
#SBATCH --array=0-34%50
#SBATCH --cpus-per-task=1
#SBATCH --mem=4G
#SBATCH --time=03:02:00
# Fallback only: submit.sh overrides these to $RUN_DIR/logs/.
#SBATCH --output=xts/results/slurm_logs/%A_%a.log
#SBATCH --error=xts/results/slurm_logs/%A_%a.err

set -euo pipefail

# Modules, venv, cd to the repo root, and run_sweep().
source "${SLURM_SUBMIT_DIR:-$(cd "$(dirname "$0")/../.." && pwd)}/xts/cluster/env.sh"

BENCH_ARGS=(--families paper --only dump-trucks --pipeline native,sc,sci,scin --array-encoding theory,uf --array-frame-mode disequality,ite --strategy seq --timeout 7200 --iterations 1 --compile-timeout 900 --compile-repeats 2 --workers 1)

run_sweep 03_dump_trucks
