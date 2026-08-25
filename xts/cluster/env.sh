# Shared setup for every cluster job — sourced by the *.sbatch sweeps and by the
# merge job submit.sh queues behind them. Keep the module versions in one place
# so a toolchain change is a one-line edit, not six.

# Lmod's init/bash references $LD_PRELOAD without a default, which trips
# "unbound variable" under set -u (nounset) -- harmless, but noisy enough to
# bury a real error underneath it. Relax -u for just the module commands.
# Load order matters: gnu12 must come before python/3.10.7, or python3 fails
# at runtime with "error while loading shared libraries: libpython3.10.so.1.0"
# (the python module's runtime dependency on the gnu12 toolchain doesn't
# resolve unless gnu12 is already active when python is loaded).
set +u
module purge
module load gnu12/12.4.0
module load python/3.10.7
set -u

source "$HOME/rtp-venv/bin/activate"

# Every path below — and in the sweep scripts — is relative to the repo root.
cd "$HOME/RanTanPlan"

# Where a sweep's output goes. RESULTS_ROOT/<experiment>/<run>/ holds
# summary.csv, raw.csv, run.json and shards/.
RESULTS_ROOT="xts/results"

# run_sweep <experiment>
#
# Runs one array task of the named sweep. The caller sets BENCH_ARGS to the
# selection flags; everything else — where output goes, the manifest, the
# per-task CSV names — is the same for every sweep and lives here rather than
# being copy-pasted six times.
run_sweep() {
    local exp="$1"

    # submit.sh exports RTP_RUN_DIR so the array and its merge job agree on a
    # dated path. The fallback uses the array job id, which is identical across
    # every task of one array — a $(date) here would split the array over two
    # directories if it ran past midnight.
    local run_dir="${RTP_RUN_DIR:-$RESULTS_ROOT/$exp/${SLURM_ARRAY_JOB_ID:-$(date +%F)}}"
    local shard_dir="$run_dir/shards"
    mkdir -p "$shard_dir"

    # Task 0 records what this sweep should produce; `bench.py --merge` verifies
    # against it, so a sweep that silently lost tasks is reported instead of
    # yielding a short summary.csv. Same BENCH_ARGS, so it cannot drift.
    if [ "${SLURM_ARRAY_TASK_ID:-0}" -eq 0 ]; then
        python3 xts/tools/bench.py "${BENCH_ARGS[@]}" \
            --write-manifest "$run_dir/run.json" || true
    fi

    python3 xts/tools/bench.py "${BENCH_ARGS[@]}" \
        --csv "$shard_dir/shard.csv" --raw-csv "$shard_dir/shard_raw.csv"
}
