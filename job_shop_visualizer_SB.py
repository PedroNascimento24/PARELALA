import tkinter as tk
import os
import matplotlib.pyplot as plt
import re
import multiprocessing

# --- Configuration for ShiftingBottleneck ---
RESULTS_DIR = 'Result/ShiftingBottleneck/'
DATA_DIR = 'Data/'
VIZ_DIR = 'viz_sb/'

RESULT_TO_DATA = {
    'P1_Small': '1_Small_sample.jss',
    'P2_Medium': '2_Medium_sample.jss',
    'P3_Large': '3_Big_sample.jss',
    'P4_XLarge': '4_XLarge_sample.jss',
}

RESULT_OPTIONS = []
for folder in RESULT_TO_DATA.keys():
    folder_path = os.path.join(RESULTS_DIR, folder)
    if os.path.isdir(folder_path):
        for file in os.listdir(folder_path):
            if file.endswith('_result.txt') and not file.endswith('_viz.txt'):
                RESULT_OPTIONS.append((folder, file))

def extract_threads_from_filename(filename):
    match = re.search(r'parallel_(\d+)threads', filename)
    if match:
        return int(match.group(1))
    if 'sequential' in filename:
        return 1
    return None

def parse_execution_time_from_log(logfile):
    with open(logfile, 'r') as f:
        for line in f:
            if 'Execution Time:' in line:
                match = re.search(r'Execution Time:\s*([0-9.]+)ms', line)
                if match:
                    return float(match.group(1))
    return None

def parse_thread_count_from_log(logfile):
    if 'sequential' in logfile:
        return 1
    match = re.search(r'parallel_(\d+)threads', logfile)
    if match:
        return int(match.group(1))
    with open(logfile, 'r') as f:
        for line in f:
            if 'Thread Count:' in line:
                match = re.search(r'Thread Count:\s*(\d+)', line)
                if match:
                    return int(match.group(1))
    return None

def generate_combined_performance_charts():
    print('Generating combined performance charts (execution time & speedup) for all sizes...')
    LOGS_DIR = 'Logs/ShiftingBottleneck/'
    VIZ_DIR = 'viz_sb/'
    sizes = [d for d in os.listdir(LOGS_DIR) if os.path.isdir(os.path.join(LOGS_DIR, d))]
    nprocs = multiprocessing.cpu_count()
    for size in sizes:
        log_dir = os.path.join(LOGS_DIR, size)
        log_files = [f for f in os.listdir(log_dir) if f.endswith('.txt')]
        sequential = None
        parallel = []
        for log_file in log_files:
            full_path = os.path.join(log_dir, log_file)
            if 'sequential' in log_file:
                exec_time = parse_execution_time_from_log(full_path)
                if exec_time is not None:
                    sequential = (1, exec_time, log_file)
            else:
                threads = parse_thread_count_from_log(log_file)
                exec_time = parse_execution_time_from_log(full_path)
                if threads is not None and exec_time is not None:
                    parallel.append((threads, exec_time, log_file))
        if sequential is None:
            print(f'No sequential result for {size}, skipping combined chart.')
            continue
        parallel.sort(key=lambda x: x[0])
        all_results = [sequential] + parallel
        t1 = sequential[1]
        ps = [1] + [t for t, et, fname in parallel]
        ts = [sequential[1]] + [et for t, et, fname in parallel]
        speedups = [1.0] + [t1 / et for t, et, fname in parallel]
        fnames = [sequential[2]] + [fname for t, et, fname in parallel]
        print(f'--- {size} ---')
        print('Threads\tExecTime(ms)\tSpeedup\t(File)')
        for t, et, s, fname in zip(ps, ts, speedups, fnames):
            label = ''
            if fname == 'sequential_log.txt':
                label = '(sequential)'
            elif t == 1:
                label = '(parallel-1thread)'
            print(f'{t}\t{et:.2f}\t\t{s:.2f}\t{label} {fname}')
        fig, axs = plt.subplots(1, 2, figsize=(14, 5))
        axs[0].plot(ps, ts, marker='o', color='red')
        for i, (x, y, fname) in enumerate(zip(ps, ts, fnames)):
            ann = 'seq' if fname == 'sequential_log.txt' else ("par-1t" if x == 1 else str(x))
            axs[0].annotate(f'{y:.1f}\n{ann}', (x, y), textcoords="offset points", xytext=(0,5), ha='center', fontsize=8)
        axs[0].set_title(f'Execution Time vs Threads\n{size}')
        axs[0].set_xlabel('Number of Threads (p)')
        axs[0].set_ylabel('Execution Time (ms)')
        axs[0].grid(True, linestyle='--', alpha=0.5)
        axs[0].set_xticks(ps)
        axs[1].plot(ps, speedups, marker='o', color='blue')
        for i, (x, y, fname) in enumerate(zip(ps, speedups, fnames)):
            ann = 'seq' if fname == 'sequential_log.txt' else ("par-1t" if x == 1 else str(x))
            axs[1].annotate(f'{y:.2f}\n{ann}', (x, y), textcoords="offset points", xytext=(0,5), ha='center', fontsize=8)
        axs[1].set_title(f'Speedup vs Threads\n{size}\n(Number of processors: {nprocs})')
        axs[1].set_xlabel('Number of Threads (p)')
        axs[1].set_ylabel('Speedup S = T1 / Tp')
        axs[1].grid(True, linestyle='--', alpha=0.5)
        axs[1].set_xticks(ps)
        plt.tight_layout()
        out_img_dir = os.path.join(VIZ_DIR, size)
        if not os.path.exists(out_img_dir):
            os.makedirs(out_img_dir)
        plt.savefig(os.path.join(out_img_dir, 'combined_performance.png'))
        plt.close()
        print(f'Combined chart saved in: {os.path.join(out_img_dir, "combined_performance.png")}')
    print('All combined performance charts generated.')

def main():
    root = tk.Tk()
    root.title('Shifting Bottleneck Visualizer UI (Result-Only)')
    root.geometry('500x200')
    tk.Label(root, text='Shifting Bottleneck Performance Charts').pack(pady=10)
    tk.Button(
        root,
        text='Generate Combined Performance Charts',
        command=generate_combined_performance_charts,
        bg='#FF9800',
        fg='white',
        font=('Arial', 12, 'bold')
    ).pack(pady=30)
    root.mainloop()

if __name__ == '__main__':
    main()