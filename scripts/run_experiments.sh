#!/usr/bin/env bash
backend=$1
method=$2
N=${3:-1000000}
range=${4:-256}
threads_list=(1 2 4 8 16 32)

# Carpeta base
mkdir -p ../results

# Carpeta por ejecución con timestamp
timestamp=$(date +%Y%m%d_%H%M%S)
run_dir="../results/run_${timestamp}_${backend}_${method}_N${N}_R${range}"
mkdir -p "$run_dir"

# CSV de salida
out="$run_dir/results.csv"
echo "method,backend,threads,rep,time_sec,total_count,valid" > $out

# Guardar info de ejecución en TXT
info_file="$run_dir/info.txt"
echo "Execution timestamp: $timestamp" > $info_file
echo "Backend: $backend" >> $info_file
echo "Method: $method" >> $info_file
echo "N: $N" >> $info_file
echo "Range: $range" >> $info_file
echo "Threads list: ${threads_list[*]}" >> $info_file

# Ejecutar experimentos
for t in "${threads_list[@]}"; do
  echo "Running ${backend} ${method} threads=${t}"
./histogram --backend ${backend} --method ${method} --threads ${t} --size ${N} --range ${range} --reps 3 | grep ',' >> $out

done

# Llamar al script de Python para graficar y guardar los PNG en la misma carpeta
python scripts/plot_results.py "$out" "$run_dir"
