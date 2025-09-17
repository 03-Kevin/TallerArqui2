#!/usr/bin/env bash
# run_scaling.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

N_big=${1:-100000000}
RANGE=${2:-256}
THREADS_LIST=(1 2 4 8 12)
REPS=${3:-3}

COMBOS=(
  "std private"
  "openmp atomic"
)

supports_openmp=false
if ./histogram --backend openmp --method private --size 1 --range 1 --threads 1 --reps 1 >/dev/null 2>&1; then
  supports_openmp=true
fi

for combo in "${COMBOS[@]}"; do
  backend=$(echo "$combo" | awk '{print $1}')
  method=$(echo "$combo" | awk '{print $2}')
  if [[ "$backend" == "openmp" && "$supports_openmp" != "true" ]]; then
    echo "Skipping $backend (not supported)"
    continue
  fi

  timestamp=$(date +%Y%m%d_%H%M%S)
  run_dir="results/run_${timestamp}_${backend}_${method}_N${N_big}_R${RANGE}"
  mkdir -p "$run_dir"
  out="$run_dir/results.csv"
  echo "method,backend,threads,rep,time_sec,total_count,valid" > "$out"

  info="$run_dir/info.txt"
  {
    echo "timestamp: $timestamp"
    echo "backend: $backend"
    echo "method: $method"
    echo "N: $N_big"
    echo "range: $RANGE"
    echo "threads_list: ${THREADS_LIST[*]}"
    echo "reps: $REPS"
    echo "compiled_with_openmp: $supports_openmp"
    echo "g++: $(g++ --version | head -n1)"
    echo "uname: $(uname -a)"
    echo "lscpu: $(lscpu)"
    echo "free: $(free -h)"
  } > "$info"

  for t in "${THREADS_LIST[@]}"; do
    echo "Running ${backend} ${method} threads=${t} N=${N_big}"

    # CSV normal
    ./histogram --backend "$backend" --method "$method" --threads "$t" --size "$N_big" --range "$RANGE" --reps "$REPS" | grep ',' >> "$out"

    # perf
    perf_data="$run_dir/perf_threads${t}.data"
    perf_report="$run_dir/perf_threads${t}_report.txt"
    perf record -o "$perf_data" -- ./histogram --backend "$backend" --method "$method" --threads "$t" --size "$N_big" --range "$RANGE" --reps "$REPS"
    perf report -i "$perf_data" --stdio > "$perf_report" || echo "perf report failed or needs permissions"
  done

  python3 scripts/plot_results.py "$out" "$run_dir"
  echo "Saved big-run in $run_dir"
done
