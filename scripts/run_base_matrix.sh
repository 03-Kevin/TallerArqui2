#!/usr/bin/env bash
# run_base_matrix.sh
# Matriz base: backends x methods, N=1e6, range=256, threads list, reps=5
set -euo pipefail

BACKENDS=("std" "openmp")
METHODS=("private" "mutex" "atomic")
N=${1:-1000000}
RANGE=${2:-256}
THREADS_LIST=(1 2 4 8 12)
REPS=${3:-5}

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

supports_openmp=false
./histogram --backend openmp --method private --size 1 --range 1 --threads 1 --reps 1 >/dev/null 2>&1 || supports_openmp=false
if ./histogram --backend openmp --method private --size 1 --range 1 --threads 1 --reps 1 >/dev/null 2>&1; then
  supports_openmp=true
fi

for backend in "${BACKENDS[@]}"; do
  if [[ "$backend" == "openmp" && "$supports_openmp" != "true" ]]; then
    echo "Skipping openmp (binary not compiled with -fopenmp)"
    continue
  fi

  for method in "${METHODS[@]}"; do
    timestamp=$(date +%Y%m%d_%H%M%S)
    run_dir="results/run_${timestamp}_${backend}_${method}_N${N}_R${RANGE}"
    mkdir -p "$run_dir"
    out="$run_dir/results.csv"
    echo "method,backend,threads,rep,time_sec,total_count,valid" > "$out"

    info="$run_dir/info.txt"
    {
      echo "timestamp: $timestamp"
      echo "backend: $backend"
      echo "method: $method"
      echo "N: $N"
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
      echo "Running ${backend} ${method} threads=${t}"

      # CSV normal
      ./histogram --backend "$backend" --method "$method" --threads "$t" --size "$N" --range "$RANGE" --reps "$REPS" | grep ',' >> "$out"

      # perf
      perf_data="$run_dir/perf_threads${t}.data"
      perf_report="$run_dir/perf_threads${t}_report.txt"
      perf record -o "$perf_data" -- ./histogram --backend "$backend" --method "$method" --threads "$t" --size "$N" --range "$RANGE" --reps "$REPS"
      perf report -i "$perf_data" --stdio > "$perf_report" || echo "perf report failed or needs permissions"
    done

    python3 scripts/plot_results.py "$out" "$run_dir"
    echo "Saved run in $run_dir"
  done
done
