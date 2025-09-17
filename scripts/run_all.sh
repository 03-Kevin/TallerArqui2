#!/usr/bin/env bash
# run_all.sh - orchestrator. Warning: long running
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# run_all.sh modificado
for N_val in 1000000 10000000 100000000; do
    for RANGE_val in 2 256 65536; do
        scripts/run_base_matrix.sh $N_val $RANGE_val 5
        scripts/run_range_sweep.sh $N_val 3
    done
done

# 3) optional: big scaling runs (uncomment if you want)
# scripts/run_scaling.sh 100000000 256 3

# 4) profiles (careful)
# scripts/run_profiles.sh 100000000 256

echo "All requested jobs launched (check results/ folders)."
