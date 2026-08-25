#!/bin/bash
#SBATCH --partition=normal
#SBATCH --job-name=rtp-scaling
#SBATCH --array=0-919%100
#SBATCH --cpus-per-task=1
#SBATCH --mem=4G
#SBATCH --time=07:32:00
# Fallback only: submit.sh overrides these to $RUN_DIR/logs/.
#SBATCH --output=xts/results/slurm_logs/%A_%a.log
#SBATCH --error=xts/results/slurm_logs/%A_%a.err

set -euo pipefail

# Modules, venv, cd to the repo root, and run_sweep().
source "${SLURM_SUBMIT_DIR:-$(cd "$(dirname "$0")/../.." && pwd)}/xts/cluster/env.sh"

BENCH_ARGS=(--families synthetic --pipeline native --array-encoding theory,uf --array-frame-mode disequality,ite --strategy seq --timeout 7200 --iterations 3 --compile-repeats 3 --compile-timeout 300 --workers 1)

run_sweep scaling
