import matplotlib.pyplot as plt
import re

# Dynamically parse log files for times
seq_files = [
    ('Small', 'logs/Small_sample_exec_seqcustom.txt'),
    ('Medium', 'logs/Medium_sample_exec_seqcustom.txt'),
    ('Big', 'logs/Big_sample_exec_seqcustom.txt'),
]
par_file = 'logs/Big_sample_exec_parcustom.txt'

seq_times = []
for label, fname in seq_files:
    try:
        with open(fname, 'r') as f:
            for line in f:
                m = re.search(r'AvgTime: ([0-9.]+)', line)
                if m:
                    seq_times.append(float(m.group(1)))
    except Exception:
        seq_times.append(0)

# For parallel, get the first occurrence for each thread count
threads = []
par_times = []
seen = set()
try:
    with open(par_file, 'r') as f:
        for line in f:
            m = re.search(r'Threads: (\d+), ParCustom, AvgTime: ([0-9.]+)', line)
            if m:
                t = int(m.group(1))
                if t not in seen:
                    threads.append(t)
                    par_times.append(float(m.group(2)))
                    seen.add(t)
except Exception:
    pass

# For plotting, align sequential time for each thread count (Big only)
if seq_times and threads:
    seq_time_big = seq_times[-1]
    seq_time_plot = [seq_time_big] * len(threads)
else:
    seq_time_plot = [0] * len(threads)

speedup = [seq/par_ if par_ > 0 else 0 for seq, par_ in zip(seq_time_plot, par_times)]

plt.figure(figsize=(10,5))
plt.subplot(1,2,1)
plt.plot(threads, seq_time_plot, label='Sequential (Big)', marker='o')
plt.plot(threads, par_times, label='Parallel (Big)', marker='o')
plt.xlabel('Threads')
plt.ylabel('Time (s)')
plt.title('Execution Time vs Threads (Big Sample)')
plt.legend()

plt.subplot(1,2,2)
plt.plot(threads, speedup, marker='o', color='green')
plt.xlabel('Threads')
plt.ylabel('Speedup')
plt.title('Speedup vs Threads (Big Sample)')
plt.tight_layout()
plt.show()
