import tkinter as tk
from tkinter import messagebox
import os
import subprocess
import sys
import matplotlib.pyplot as plt
import random
import matplotlib.patches as patches
from matplotlib.patheffects import withStroke
import glob
import re
import multiprocessing

# --- Configuration: Set your default options here ---
RESULTS_DIR = 'Result/BranchAndBound/'
DATA_DIR = 'Data/'
VIZ_DIR = 'viz/'

# Map result folders to their corresponding data files
RESULT_TO_DATA = {
    'P1_Small': '1_Small_sample.jss',
    'P2_Medium': '2_Medium_sample.jss',
    'P3_Large': '3_Big_sample.jss',
    'P4_XLarge': '4_XLarge_sample.jss',
}

# Dynamically build RESULT_OPTIONS (show all results, not limited to 4)
RESULT_OPTIONS = []
for folder in RESULT_TO_DATA.keys():
    folder_path = os.path.join(RESULTS_DIR, folder)
    if os.path.isdir(folder_path):
        for file in os.listdir(folder_path):
            if file.endswith('_result.txt') and not file.endswith('_viz.txt'):
                RESULT_OPTIONS.append((folder, file))

def read_job_shop_file(filename):
    with open(filename, 'r') as file:
        lines = file.readlines()
    makespan = int(lines[0].strip())
    jobs_start_times = []
    job_durations = []
    for i in range(1, len(lines)):
        if lines[i].strip():
            pairs = lines[i].strip().split()
            start_times = []
            durations = []
            for pair in pairs:
                start, duration = map(int, pair.split(','))
                start_times.append(start)
                durations.append(duration)
            jobs_start_times.append(start_times)
            job_durations.append(durations)
    return makespan, jobs_start_times, job_durations

def parse_jss_file(jss_file):
    with open(jss_file) as f:
        lines = [line.strip() for line in f if line.strip()]
    njobs, nops = map(int, lines[0].split())
    plan = []
    for line in lines[1:]:
        nums = list(map(int, line.split()))
        ops = [(nums[i], nums[i+1]) for i in range(0, len(nums), 2)]
        plan.append(ops)
    return plan

def convert_result_to_start_duration(jobs_file, input_jss_file, out_file):
    with open(jobs_file) as f:
        lines = [line.strip() for line in f if line.strip()]
    if ',' in lines[1]:
        return jobs_file  # Already in correct format
    plan = parse_jss_file(input_jss_file)
    new_lines = [lines[0]]
    for job_idx, line in enumerate(lines[1:]):
        starts = list(map(int, line.split()))
        pairs = [f'{starts[op_idx]},{plan[job_idx][op_idx][1]}' for op_idx in range(len(starts))]
        new_lines.append(' '.join(pairs))
    with open(out_file, 'w') as f:
        f.write('\n'.join(new_lines) + '\n')
    return out_file

def create_dynamic_machine_mapping(jss_file):
    plan = parse_jss_file(jss_file)
    mapping = []
    for job_ops in plan:
        machines = [op[0] for op in job_ops]
        mapping.append(machines)
    return mapping

def verify_no_overlap(machine_timeframes):
    for machine, operations in machine_timeframes.items():
        sorted_ops = sorted(operations, key=lambda x: x['start'])
        for i in range(1, len(sorted_ops)):
            prev_end = sorted_ops[i-1]['start'] + sorted_ops[i-1]['duration']
            curr_start = sorted_ops[i]['start']
            if curr_start < prev_end:
                print(f"Overlap detected on machine {machine}!")
                return False
    return True

def create_job_shop_gantt(jobs_file, mapping, output_image):
    # Ensure output directory exists
    output_dir = os.path.dirname(output_image)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)
    makespan, jobs_start_times, job_durations = read_job_shop_file(jobs_file)
    # Check mapping dimensions
    for job_idx, (start_times, machines) in enumerate(zip(jobs_start_times, mapping)):
        if len(start_times) != len(machines):
            if len(machines) < len(start_times):
                extra_machines = [m % len(machines) for m in range(len(machines), len(start_times))]
                mapping[job_idx].extend(extra_machines)
            elif len(machines) > len(start_times):
                mapping[job_idx] = machines[:len(start_times)]
    machines_used = set(machine for job_assignments in mapping for machine in job_assignments)
    num_machines = max(machines_used) + 1
    machine_operations = {i: [] for i in range(num_machines)}
    for job_idx, (start_times, durations, machine_assignments) in enumerate(
            zip(jobs_start_times, job_durations, mapping)):
        for op_idx, (start, duration, machine) in enumerate(
                zip(start_times, durations, machine_assignments)):
            if op_idx < len(machine_assignments):
                machine_operations[machine].append({
                    'job': job_idx,
                    'operation': op_idx,
                    'start': start,
                    'duration': duration
                })
    verify_no_overlap(machine_operations)
    colors = ['#e6194b', '#3cb44b', '#0082c8', '#f58231', '#911eb4', '#46f0f0']
    if len(jobs_start_times) > len(colors):
        for _ in range(len(jobs_start_times) - len(colors)):
            hex_color = "#{:06x}".format(random.randint(0, 0xFFFFFF))
            colors.append(hex_color)
    fig, ax = plt.subplots(figsize=(16, 8))
    bar_height = 0.6
    y_spacing = 1.2
    for machine_idx, operations in sorted(machine_operations.items()):
        y_position = machine_idx * y_spacing
        for op in operations:
            job_idx = op['job']
            op_idx = op['operation']
            start = op['start']
            duration = op['duration']
            rect = patches.Rectangle(
                (start, y_position - bar_height/2),
                duration,
                bar_height,
                linewidth=1,
                edgecolor='black',
                facecolor=colors[job_idx % len(colors)],
                alpha=0.85
            )
            ax.add_patch(rect)
            label = f'j{job_idx},o{op_idx}'
            ax.text(
                start + duration/2,
                y_position,
                label,
                ha='center',
                va='center',
                fontsize=7,
                color='white',
                rotation=45,
                path_effects=[withStroke(linewidth=2, foreground='black')]
            )
            if duration > 2:
                ax.text(start, y_position - bar_height/2 - 0.1, f'{start}', ha='center', va='top', fontsize=6, color='black')
                ax.text(start + duration, y_position - bar_height/2 - 0.1, f'{start + duration}', ha='center', va='top', fontsize=6, color='black')
    ax.set_yticks([i * y_spacing for i in range(num_machines)])
    ax.set_yticklabels([f'Machine {i}' for i in range(num_machines)], fontsize=10)
    ax.set_xlim(0, makespan * 1.05)
    ax.set_ylim(-0.5, num_machines * y_spacing)
    ax.grid(True, which='both', linestyle='--', alpha=0.5)
    ax.set_xlabel('Time', fontsize=12)
    ax.set_title(f'Job Shop Schedule - {jobs_file} (Makespan: {makespan})', fontsize=14, pad=20)
    handles = [patches.Patch(color=colors[j % len(colors)], label=f'Job {j}') 
               for j in range(len(jobs_start_times))]
    ax.legend(handles=handles, loc='center left', bbox_to_anchor=(1, 0.5), fontsize=10)
    plt.tight_layout()
    plt.subplots_adjust(right=0.85)
    plt.savefig(output_image, dpi=200, bbox_inches='tight')
    plt.close()
    return makespan

def run_visualization(result_key):
    result_folder, result_file = result_key
    jobs_file = os.path.join(RESULTS_DIR, result_folder, result_file)
    input_jss_file = os.path.join(DATA_DIR, RESULT_TO_DATA[result_folder])
    out_img_dir = os.path.join(VIZ_DIR, result_folder)
    if not os.path.exists(out_img_dir):
        os.makedirs(out_img_dir)
    out_img = os.path.join(out_img_dir, f'gantt_{result_file.replace(".txt","")}.png')
    jobs_file_for_viz = convert_result_to_start_duration(jobs_file, input_jss_file, jobs_file + '_viz.txt')
    mapping = create_dynamic_machine_mapping(input_jss_file)
    makespan = create_job_shop_gantt(jobs_file_for_viz, mapping, out_img)
    print(f'Gantt chart generated: {out_img} | Makespan: {makespan}')
    # No messagebox, no open image

def extract_threads_from_filename(filename):
    import re
    match = re.search(r'parallel_(\d+)threads', filename)
    if match:
        return int(match.group(1))
    if 'sequential' in filename:
        return 1
    return None

def generate_speedup_charts():
    print('Generating speedup charts for all sizes...')
    import matplotlib.pyplot as plt
    for size in RESULT_TO_DATA.keys():
        results = []
        for f, file in RESULT_OPTIONS:
            if f == size:
                threads = extract_threads_from_filename(file)
                if threads is not None:
                    jobs_file = os.path.join(RESULTS_DIR, f, file)
                    input_jss_file = os.path.join(DATA_DIR, RESULT_TO_DATA[f])
                    jobs_file_for_viz = convert_result_to_start_duration(jobs_file, input_jss_file, jobs_file + '_viz.txt')
                    makespan, *_ = read_job_shop_file(jobs_file_for_viz)
                    results.append((threads, makespan, file))
        if not results:
            print(f'No results for {size}')
            continue
        # Sort by thread count
        results.sort(key=lambda x: x[0])
        t1 = next((m for t, m, fname in results if t == 1 or "sequential" in fname), None)
        if t1 is None:
            print(f'No sequential result for {size}, skipping speedup chart.')
            continue
        ps = [t for t, m, fname in results if t > 0]
        ts = [m for t, m, fname in results if t > 0]
        speedups = [t1 / m for m in ts]
        # Print table
        print(f'--- {size} ---')
        print('Threads\tMakespan\tSpeedup')
        for t, m, s, fname in zip(ps, ts, speedups, [fname for t, m, fname in results if t > 0]):
            print(f'{t}\t{m}\t{s:.2f}\t({fname})')
        # Plot
        plt.figure(figsize=(8,5))
        plt.plot(ps, speedups, marker='o', color='blue')
        plt.title(f'Speedup for {size}')
        plt.xlabel('Number of Threads (p)')
        plt.ylabel('Speedup S = T1 / Tp')
        plt.grid(True, linestyle='--', alpha=0.5)
        plt.xticks(ps)
        plt.tight_layout()
        out_img_dir = os.path.join(VIZ_DIR, size)
        if not os.path.exists(out_img_dir):
            os.makedirs(out_img_dir)
        plt.savefig(os.path.join(out_img_dir, 'speedup.png'))
        plt.close()
        print(f'Speedup chart saved: {os.path.join(out_img_dir, "speedup.png")}')
    print('All speedup charts generated.')

def parse_execution_time_from_log(logfile):
    with open(logfile, 'r') as f:
        for line in f:
            if 'Execution Time:' in line:
                # Handles both '23ms' and '23.51ms'
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
    # Fallback: try to read from inside the file
    with open(logfile, 'r') as f:
        for line in f:
            if 'Thread Count:' in line:
                match = re.search(r'Thread Count:\s*(\d+)', line)
                if match:
                    return int(match.group(1))
    return None

def generate_combined_performance_charts():
    print('Generating combined performance charts (execution time & speedup) for all sizes...')
    LOGS_DIR = 'Logs/BranchAndBound/'
    VIZ_DIR = 'viz/'
    sizes = [d for d in os.listdir(LOGS_DIR) if os.path.isdir(os.path.join(LOGS_DIR, d))]
    import multiprocessing
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
        # Print table
        print(f'--- {size} ---')
        print('Threads\tExecTime(ms)\tSpeedup\t(File)')
        for t, et, s, fname in zip(ps, ts, speedups, fnames):
            label = ''
            if fname == 'sequential_log.txt':
                label = '(sequential)'
            elif t == 1:
                label = '(parallel-1thread)'
            print(f'{t}\t{et:.2f}\t\t{s:.2f}\t{label} {fname}')
        # Combined plot
        import matplotlib.pyplot as plt
        fig, axs = plt.subplots(1, 2, figsize=(14, 5))
        # Execution time plot
        axs[0].plot(ps, ts, marker='o', color='red')
        for i, (x, y, fname) in enumerate(zip(ps, ts, fnames)):
            ann = 'seq' if fname == 'sequential_log.txt' else ("par-1t" if x == 1 else str(x))
            axs[0].annotate(f'{y:.1f}\n{ann}', (x, y), textcoords="offset points", xytext=(0,5), ha='center', fontsize=8)
        axs[0].set_title(f'Execution Time vs Threads\n{size}')
        axs[0].set_xlabel('Number of Threads (p)')
        axs[0].set_ylabel('Execution Time (ms)')
        axs[0].grid(True, linestyle='--', alpha=0.5)
        axs[0].set_xticks(ps)
        # Speedup plot
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
    root.title('Job Shop Visualizer UI (Result-Only)')
    root.geometry('500x350')
    tk.Label(root, text='Select Problem Size:').pack(pady=5)
    size_var = tk.StringVar()
    size_options = [folder for folder in RESULT_TO_DATA.keys() if any(folder == f for f, _ in RESULT_OPTIONS)]
    if not size_options:
        tk.Label(root, text='No results found!').pack()
        root.mainloop()
        return
    size_var.set(size_options[0])
    for folder in size_options:
        tk.Radiobutton(root, text=folder, variable=size_var, value=folder).pack(anchor='w')
    def on_visualize():
        selected_size = size_var.get()
        files = [file for f, file in RESULT_OPTIONS if f == selected_size]
        if not files:
            print(f'No results for {selected_size}.')
            return
        print(f'Generating Gantt charts for {selected_size}:')
        for file in files:
            run_visualization((selected_size, file))
        print(f'Batch complete for {selected_size}.')
    tk.Button(root, text='Visualize Gantt Charts', command=on_visualize, bg='#4CAF50', fg='white', font=('Arial', 12, 'bold')).pack(pady=10)
    # Replace two buttons with one for combined charts
    tk.Button(root, text='Generate Combined Performance Charts', command=generate_combined_performance_charts, bg='#FF9800', fg='white', font=('Arial', 12, 'bold')).pack(pady=10)
    root.mainloop()

if __name__ == '__main__':
    main()
