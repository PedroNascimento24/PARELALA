import tkinter as tk
from tkinter import filedialog, messagebox
import os
import subprocess
import sys

# Import the visualization logic from job_shop_visualizer.py
import job_shop_visualizer

# --- Configuration: Set your default options here ---
RESULTS_DIR = 'Result/BranchAndBound/'
DATA_DIR = 'Data/'
MAPPINGS_DIR = 'mappings/'
VIZ_DIR = 'viz/'

# List of result files (update as needed)
RESULT_OPTIONS = [
    ('P1_Small', 'parallel_1threads_result.txt'),
    ('P2_Medium', 'parallel_2threads_result.txt'),
    ('P3_Large', 'parallel_4threads_result.txt'),
    ('P4_XLarge', 'parallel_8threads_result.txt'),
]

DATA_OPTIONS = [
    ('1_Small_sample.jss'),
    ('2_Medium_sample.jss'),
    ('3_Big_sample.jss'),
    ('4_XLarge_sample.jss'),
]

MAPPING_OPTIONS = [
    ('mapping_small.txt'),
    ('mapping_medium.txt'),
    ('mapping_big.txt'),
    ('mapping_xlarge.txt'),
]

def get_full_path(folder, filename):
    return os.path.join(folder, filename)

def run_visualization(result_key, data_file, mapping_file):
    # Compose full paths
    result_folder, result_file = result_key
    jobs_file = os.path.join(RESULTS_DIR, result_folder, result_file)
    input_jss_file = os.path.join(DATA_DIR, data_file)
    mapping_path = os.path.join(MAPPINGS_DIR, mapping_file) if mapping_file else None
    # Output image
    out_img = os.path.join(VIZ_DIR, f'gantt_{result_folder}.png')
    # Convert if needed
    jobs_file_for_viz = job_shop_visualizer.main.convert_result_to_start_duration(
        jobs_file, input_jss_file, jobs_file + '_viz.txt')
    # Run visualization
    makespan = job_shop_visualizer.create_job_shop_gantt(jobs_file_for_viz, mapping_path, out_img)
    messagebox.showinfo('Visualization Complete', f'Gantt chart generated: {out_img}\nMakespan: {makespan}')
    # Optionally, open the image
    try:
        if sys.platform.startswith('win'):
            os.startfile(out_img)
        else:
            subprocess.Popen(['xdg-open', out_img])
    except Exception:
        pass

def main():
    root = tk.Tk()
    root.title('Job Shop Visualizer UI')
    root.geometry('500x350')

    # --- Result selection ---
    tk.Label(root, text='Select Branch & Bound Result:').pack(pady=5)
    result_var = tk.StringVar(value=RESULT_OPTIONS[0][0])
    for folder, file in RESULT_OPTIONS:
        tk.Radiobutton(root, text=f'{folder} ({file})', variable=result_var, value=folder).pack(anchor='w')

    # --- Data selection ---
    tk.Label(root, text='Select Data Sample:').pack(pady=5)
    data_var = tk.StringVar(value=DATA_OPTIONS[0])
    for data_file in DATA_OPTIONS:
        tk.Radiobutton(root, text=data_file, variable=data_var, value=data_file).pack(anchor='w')

    # --- Mapping selection ---
    tk.Label(root, text='Select Mapping File:').pack(pady=5)
    mapping_var = tk.StringVar(value=MAPPING_OPTIONS[0])
    for mapping_file in MAPPING_OPTIONS:
        tk.Radiobutton(root, text=mapping_file, variable=mapping_var, value=mapping_file).pack(anchor='w')

    # --- Visualize button ---
    def on_visualize():
        # Find selected result tuple
        selected_result = next((item for item in RESULT_OPTIONS if item[0] == result_var.get()), RESULT_OPTIONS[0])
        run_visualization(selected_result, data_var.get(), mapping_var.get())

    tk.Button(root, text='Visualize', command=on_visualize, bg='#4CAF50', fg='white', font=('Arial', 12, 'bold')).pack(pady=20)

    root.mainloop()

if __name__ == '__main__':
    main()
