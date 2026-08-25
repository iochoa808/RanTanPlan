#!/bin/bash
#SBATCH --partition=normal
#SBATCH --job-name=rtp-02_labyrinth_rushhour_sokoban
#SBATCH --array=0-419%100
#SBATCH --cpus-per-task=1
#SBATCH --mem=4G
#SBATCH --time=02:44:00
# Fallback only: submit.sh overrides these to $RUN_DIR/logs/.
#SBATCH --output=xts/results/slurm_logs/%A_%a.log
#SBATCH --error=xts/results/slurm_logs/%A_%a.err

set -euo pipefail

# Modules, venv, cd to the repo root, and run_sweep().
source "${SLURM_SUBMIT_DIR:-$(cd "$(dirname "$0")/../.." && pwd)}/xts/cluster/env.sh"

BENCH_ARGS=(--families paper --only labyrinth rush-hour sokoban --pipeline native,up,uti --array-encoding theory,uf --array-frame-mode disequality,ite --strategy seq --timeout 7200 --iterations 1 --compile-timeout 300 --workers 1)

run_sweep 02_labyrinth_rushhour_sokoban
