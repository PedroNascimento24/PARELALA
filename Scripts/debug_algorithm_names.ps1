python -c "
import sys
sys.path.append('../Python')
from c_output_gantt import *

# Read the result file
result_file = '../Result/ShiftingBottleneck/P5_XXLarge/parallel_16threads_result.txt'
with open(result_file, 'r') as f:
    lines = f.readlines()

# Parse the schedule
jobs = []
machines = {}
makespan = 0

for line in lines[6:]:  # Skip header lines
    if line.strip() and line.startswith('Job'):
        parts = line.strip().split()
        job_id = int(parts[1].rstrip(','))
        op_id = int(parts[3].rstrip(':'))
        machine = parts[4].rstrip(',')
        duration = int(parts[6].rstrip(','))
        start_time = int(parts[8])
        
        end_time = start_time + duration
        makespan = max(makespan, end_time)
        
        # Store operation
        jobs.append((job_id, op_id, machine, duration, start_time, end_time))
        
        # Group by machine
        if machine not in machines:
            machines[machine] = []
        machines[machine].append((job_id, op_id, start_time, end_time))

print(f'Total operations: {len(jobs)}')
print(f'Calculated makespan: {makespan}')

# Check for machine overlaps
overlap_count = 0
for machine, ops in machines.items():
    # Sort operations by start time
    ops.sort(key=lambda x: x[2])
    
    for i in range(len(ops) - 1):
        current_end = ops[i][3]
        next_start = ops[i+1][2]
        
        if current_end > next_start:
            overlap_count += 1
            print(f'OVERLAP on {machine}: Job {ops[i][0]} Op {ops[i][1]} (ends {current_end}) overlaps with Job {ops[i+1][0]} Op {ops[i+1][1]} (starts {next_start})')

print(f'Total machine overlaps found: {overlap_count}')

# Check job precedence violations
job_schedules = {}
for job_id, op_id, machine, duration, start_time, end_time in jobs:
    if job_id not in job_schedules:
        job_schedules[job_id] = []
    job_schedules[job_id].append((op_id, start_time, end_time))

precedence_violations = 0
for job_id, operations in job_schedules.items():
    # Sort operations by operation ID
    operations.sort(key=lambda x: x[0])
    
    for i in range(len(operations) - 1):
        current_end = operations[i][2]
        next_start = operations[i+1][1]
        
        if current_end > next_start:
            precedence_violations += 1
            print(f'PRECEDENCE VIOLATION in Job {job_id}: Op {operations[i][0]} (ends {current_end}) > Op {operations[i+1][0]} (starts {next_start})')

print(f'Total precedence violations found: {precedence_violations}')

if overlap_count == 0 and precedence_violations == 0:
    print('✓ VALID SCHEDULE: No overlaps or precedence violations found')
else:
    print('✗ INVALID SCHEDULE: Found violations')
"