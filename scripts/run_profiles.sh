#!/usr/bin/env bash
# run_profiles.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

THREADS=8
N_prof=${1:-100000000}
RANGE=${2:-256}

CASES=("std mutex" "std private" "openmp atomic")

supports_openmp=false
if ./histogram --backend openmp --method private --size 1 --range 1 --threads 1 --reps 1 >/dev/null 2>&1; then
  supports_openmp=true
fi

for case in "${CASES[@]}"; do
  backend=$(echo "$case" | awk '{print $1}')
  method=$(echo "$case" | awk '{print $2}')
  if [[ "$backend" == "openmp" && "$supports_openmp" != "true" ]]; then
    echo "Skipping openmp case (binary not compiled with -fopenmp)"
    continue
  fi

  timestamp=$(date +%Y%m%d_%H%M%S)
  run_dir="results/run_profile_${timestamp}_${backend}_${method}_N${N_prof}_T${THREADS}"
  mkdir -p "$run_dir"

  time_out="$run_dir/time_${backend}_${method}_t${THREADS}.txt"
  echo "Running /usr/bin/time -v for $backend $method"
  /usr/bin/time -v ./histogram --backend "$backend" --method "$method" --threads "$THREADS" --size "$N_prof" --range "$RANGE" --reps 1 --seed 12345 > /dev/null 2> "$time_out"

  perf_data="$run_dir/perf_threads${THREADS}.data"
  perf_report="$run_dir/perf_threads${THREADS}_report.txt"
  echo "Running perf record for $backend $method"
  perf record -o "$perf_data" -- ./histogram --backend "$backend" --method "$method" --threads "$THREADS" --size "$N_prof" --range "$RANGE" --reps 1 --seed 12345
  perf report -i "$perf_data" --stdio > "$perf_report" || echo "perf report failed or needs permissions"

  echo "Saved profile results in $run_dir"
done
