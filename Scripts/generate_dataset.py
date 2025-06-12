#!/usr/bin/env python3
"""
Job Shop Scheduling Dataset Generator
Generates large datasets in the JSS format for performance testing.
"""

import random
import sys

def generate_jobshop_dataset(num_jobs, num_machines, operations_per_job, filename, seed=42):
    """
    Generate a Job Shop Scheduling dataset
    
    Args:
        num_jobs: Number of jobs
        num_machines: Number of machines 
        operations_per_job: Number of operations per job
        filename: Output filename
        seed: Random seed for reproducibility
    """
    random.seed(seed)
    
    with open(filename, 'w') as f:
        # Write header: num_jobs num_machines
        f.write(f"{num_jobs} {num_machines}\n")
        
        # Generate each job
        for job_id in range(num_jobs):
            operations = []
            
            # Create a random permutation of machines for this job
            machines = list(range(num_machines))
            random.shuffle(machines)
            
            # Generate operations (machine, processing_time) pairs
            for op_id in range(operations_per_job):
                machine = machines[op_id % num_machines]  # Ensure we use different machines
                processing_time = random.randint(1, 100)  # Random processing time 1-100
                operations.append(f"{machine} {processing_time}")
            
            # Write job operations
            f.write(" ".join(operations) + "\n")
    
    print(f"Generated {filename}: {num_jobs} jobs × {num_machines} machines × {operations_per_job} ops/job = {num_jobs * operations_per_job} total operations")

def main():
    if len(sys.argv) < 4:
        print("Usage: python generate_dataset.py <num_jobs> <num_machines> <operations_per_job> [output_file] [seed]")
        print("Example: python generate_dataset.py 100 100 100 6_XXXLarge_sample.jss 42")
        sys.exit(1)
    
    num_jobs = int(sys.argv[1])
    num_machines = int(sys.argv[2]) 
    operations_per_job = int(sys.argv[3])
    output_file = sys.argv[4] if len(sys.argv) > 4 else f"dataset_{num_jobs}x{num_machines}x{operations_per_job}.jss"
    seed = int(sys.argv[5]) if len(sys.argv) > 5 else 42
    
    generate_jobshop_dataset(num_jobs, num_machines, operations_per_job, output_file, seed)

if __name__ == "__main__":
    main()
