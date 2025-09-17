# Taller01 - OpenMP vs std::thread - Histogram

## Requisitos
- Linux 
- make, python3, pip packages: pandas, matplotlib
- (opcional) perf para perfilado

## Compilar
# versión solo std::thread
make

# versión con OpenMP
make OPENMP=1

## Ejecución
# backend std::thread, método private, 4 threads, N=100M, range=256
./histogram --backend std --method private --threads 4 --size 100000000 --range 256 --seed 12345 --reps 5

# backend openmp, método atomic, 8 threads
./histogram --backend openmp --method atomic --threads 8 --size 100000000 --range 256 --seed 12345 --reps 5

## Ejecutar lote de experimentos
cd scripts
./run_experiments.sh std private 100000000 256
# luego graficar
python3 plot_results.py ../results/std_private_N100000000_R256.csv

## Validación
El programa imprime líneas CSV con:
method,backend,threads,rep,time_sec,total_count,OK/BAD

-- Verificar que total_count == N y OK

## Perfilado
- Recomendado usar `perf record -o perf.data ./histogram ...` y `perf report`.
- Evitar VM para mediciones confiables.

## Notas
- Para reproducibilidad usar la misma semilla y parámetros.
- Ajustar número de threads al número de HW threads físicos/hyperthreads.
