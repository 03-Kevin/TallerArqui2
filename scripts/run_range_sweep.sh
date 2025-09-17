#!/usr/bin/env bash
# run_range_sweep.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

N=${1:-1000000}
THREADS_LIST=(1 2 4 8 12)
REPS=${2:-3}
RANGES=(2 256 65536)

for range in "${RANGES[@]}"; do
  for method in atomic private; do
    timestamp=$(date +%Y%m%d_%H%M%S)
    run_dir="results/run_${timestamp}_std_${method}_N${N}_R${range}"
    mkdir -p "$run_dir"
    out="$run_dir/results.csv"
    echo "method,backend,threads,rep,time_sec,total_count,valid" > "$out"
    echo "N=$N range=$range method=$method" > "$run_dir/info.txt"

    for t in "${THREADS_LIST[@]}"; do
      echo "Running std ${method} threads=${t} range=${range}"

      # CSV normal
      ./histogram --backend std --method "$method" --threads "$t" --size "$N" --range "$range" --reps "$REPS" | grep ',' >> "$out"

      # perf
      perf_data="$run_dir/perf_threads${t}.data"
      perf_report="$run_dir/perf_threads${t}_report.txt"
      perf record -o "$perf_data" -- ./histogram --backend std --method "$method" --threads "$t" --size "$N" --range "$range" --reps "$REPS"
      perf report -i "$perf_data" --stdio > "$perf_report" || echo "perf report failed or needs permissions"
    done

    python3 scripts/plot_results.py "$out" "$run_dir"
  done
done
