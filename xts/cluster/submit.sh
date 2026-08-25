#!/usr/bin/env bash
# Submit one sweep and its merge together.
#
#     xts/cluster/submit.sh 01_pancake_15puzzle.sbatch
#
# Queues the array job, then queues a merge job that waits on it
# (--dependency=afterany). When the array finishes you have
# xts/results/<experiment>/<date>/{summary.csv,raw.csv} without touching anything.
#
# afterany, not afterok: if some tasks die, you still want the merge to run and
# tell you which indices to requeue. It exits non-zero in that case, so the
# merge job's own state shows the sweep was incomplete.
set -euo pipefail

cd "$(dirname "$0")/../.."
ROOT="$PWD"

script="${1:?usage: xts/cluster/submit.sh <script.sbatch>}"
[ -f "$script" ] || script="xts/cluster/$(basename "$script")"
[ -f "$script" ] || { echo "no such sweep script: $1" >&2; exit 1; }

exp="$(basename "$script" .sbatch)"
run_dir="xts/results/$exp/$(date +%F)"

# Pre-flight: the #SBATCH --array size is written by hand and can drift from what
# the selection actually enumerates. When it does, every task exits "--job-index
# out of range" and you burn a whole array to learn it. Worse, a broken corpus
# (say a missing UP helper module) silently makes the selection empty. Check
# before submitting, not after.
bench_args="$(sed -n 's/^BENCH_ARGS=(\(.*\))$/\1/p' "$script")"
if [ -z "$bench_args" ]; then
    echo "warning: no BENCH_ARGS=(...) line in $script — skipping the job-count check." >&2
else
    # shellcheck disable=SC2086
    n_jobs=$(python3 xts/tools/bench.py $bench_args --count-jobs 2>/dev/null | grep -oE '^[0-9]+' || true)
    array_spec=$(grep -oE '^#SBATCH --array=[0-9]+-[0-9]+' "$script" | grep -oE '[0-9]+-[0-9]+' | head -1)
    declared=$(( ${array_spec#*-} + 1 ))

    if [ -z "${n_jobs:-}" ] || [ "$n_jobs" -eq 0 ]; then
        echo "refusing to submit: the selection in $script enumerates 0 jobs." >&2
        echo "Every one of the $declared array tasks would fail out-of-range. Run" >&2
        echo "    python3 xts/tools/bench.py $bench_args --list" >&2
        echo "to see what it resolves to — usually a corpus path or a UP module is missing." >&2
        exit 1
    fi
    if [ "$n_jobs" -ne "$declared" ]; then
        echo "refusing to submit: $script declares --array=$array_spec ($declared tasks)" >&2
        echo "but its selection enumerates $n_jobs jobs." >&2
        echo "Fix the #SBATCH --array line to 0-$((n_jobs - 1)), then resubmit." >&2
        exit 1
    fi
    echo "pre-flight : $n_jobs jobs, matches --array=$array_spec"
fi

if [ -e "$run_dir" ]; then
    echo "note: $run_dir already exists — a sweep was already submitted today."
    echo "      Shards from both will merge together. Move it aside first if"
    echo "      that is not what you want."
fi
mkdir -p "$run_dir/shards" "$run_dir/logs" xts/results/slurm_logs

# RTP_RUN_DIR is what makes both jobs agree on the output location. Without it
# the sweep script falls back to the array job id, which is stable across tasks
# but is not the name this script just computed.
# --output/--error here override the #SBATCH fallback in the script, so the
# array's logs land in the run directory next to the shards they produced.
array_id=$(sbatch --parsable --export=ALL,RTP_RUN_DIR="$run_dir" \
    --output="$run_dir/logs/%A_%a.log" --error="$run_dir/logs/%A_%a.err" \
    "$script")

merge_id=$(sbatch --parsable \
    --dependency=afterany:"$array_id" \
    --job-name="rtp-merge-$exp" \
    --output="$run_dir/logs/merge_%j.log" \
    --error="$run_dir/logs/merge_%j.err" \
    --time=00:20:00 --mem=2G --cpus-per-task=1 \
    --wrap "source $ROOT/xts/cluster/env.sh && python3 xts/tools/bench.py --merge $run_dir")

echo "array job : $array_id"
echo "merge job : $merge_id   (starts when the array finishes)"
echo "output    : $run_dir/{summary.csv,raw.csv}"
echo "logs      : $run_dir/logs/"
echo
echo "Watch:   squeue -j $array_id,$merge_id"
echo "Merge log when done:   $run_dir/logs/merge_$merge_id.log"
echo "Merge by hand at any point (safe to repeat):"
echo "    python3 xts/tools/bench.py --merge $run_dir"