#!/bin/bash
#SBATCH --partition=normal
#SBATCH --job-name=rtp-05_paper_handcrafted
#SBATCH --array=0-347%100
#SBATCH --cpus-per-task=1
#SBATCH --mem=4G
#SBATCH --time=03:02:00
# Fallback only: submit.sh overrides these to $RUN_DIR/logs/.
#SBATCH --output=xts/results/slurm_logs/%A_%a.log
#SBATCH --error=xts/results/slurm_logs/%A_%a.err

set -euo pipefail

# Modules, venv, cd to the repo root, and run_sweep().
source "${SLURM_SUBMIT_DIR:-$(cd "$(dirname "$0")/../.." && pwd)}/xts/cluster/env.sh"

BENCH_ARGS=(--families paper-hc --pipeline native --array-encoding theory --array-frame-mode disequality --strategy seq --timeout 7200 --iterations 1 --compile-timeout 1800 --compile-repeats 1 --workers 1)

run_sweep 05_paper_handcrafted
