import sys
import pandas as pd
import matplotlib.pyplot as plt
import os

if len(sys.argv) < 3:
    print("Usage: plot_results.py results_csv output_dir")
    sys.exit(1)

fn = sys.argv[1]
out_dir = sys.argv[2]

df = pd.read_csv(fn)
df["threads"] = df["threads"].astype(int)
df["time_sec"] = df["time_sec"].astype(float)

grp = df.groupby("threads")["time_sec"].median().reset_index()
threads = grp["threads"]
times = grp["time_sec"]

t1 = float(times[threads==1])
speedup = t1 / times
eff = speedup / threads

# Crear gráficos en la carpeta
os.makedirs(out_dir, exist_ok=True)

plt.figure()
plt.plot(threads, times, marker='o')
plt.xlabel("HW threads")
plt.ylabel("Time (s)")
plt.title("Execution time vs threads")
plt.grid(True)
plt.savefig(os.path.join(out_dir, "time_vs_threads.png"))

plt.figure()
plt.plot(threads, speedup, marker='o')
plt.xlabel("HW threads")
plt.ylabel("Speedup")
plt.grid(True)
plt.savefig(os.path.join(out_dir, "speedup.png"))

plt.figure()
plt.plot(threads, eff, marker='o')
plt.xlabel("HW threads")
plt.ylabel("Efficiency")
plt.grid(True)
plt.savefig(os.path.join(out_dir, "efficiency.png"))

print(f"Saved plots in {out_dir}")
