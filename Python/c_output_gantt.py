import sys
import os
from chart import create_gantt_chart

def c_output_to_schedule(c_output_file):
    with open(c_output_file, 'r') as f:
        lines = [line.strip() for line in f if line.strip()]
    makespan = int(lines[0].split()[0])
    jobs = []
    for job_id, line in enumerate(lines[1:]):
        pairs = line.split()
        for op_idx, pair in enumerate(pairs):
            # Accept start,duration or start,duration,machine
            parts = pair.split(',')
            if len(parts) == 2:
                start, duration = map(int, parts)
                machine = op_idx  # fallback: use op_idx as machine
            elif len(parts) == 3:
                start, duration, machine = map(int, parts)
            else:
                continue  # skip malformed
            jobs.append((start, job_id, duration, {"machine": machine}))
    return {'jobs': jobs}

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python c_output_gantt.py <c_output_file> [output_image.png]")
        sys.exit(1)
    c_output_file = sys.argv[1]
    output_image = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(c_output_file)[0] + '_gantt.png'
    schedule = c_output_to_schedule(c_output_file)
    create_gantt_chart(schedule, output_image)
    print(f"Gantt chart saved to {output_image}")
