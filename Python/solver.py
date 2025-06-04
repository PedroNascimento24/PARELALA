import collections
from ortools.sat.python import cp_model
from maker import DatasetParser
from chart import create_gantt_chart

def process_file(file_path):
    """
    Processes the selected file and solves the jobshop problem.
    Args:
        file_path (str): The path to the input file containing the jobshop problem data.
    Returns:
        None
    The function performs the following steps:
    1. Loads the dataset from the specified file.
    2. Extracts job durations, precedence relations, and resource availability from the dataset.
    3. Creates a constraint programming model to solve the jobshop scheduling problem.
    4. Defines decision variables for job start times, end times, and intervals.
    5. Sets the objective function to minimize the makespan.
    6. Adds precedence and resource constraints to the model.
    7. Solves the model using a CP-SAT solver.
    8. If a solution is found, prints the optimal schedule length and the job assignments.
    9. Generates a Gantt chart of the schedule and saves it as 'gantt_chart.png'.
    10. Prints statistics about the solving process.
    Note:
        The function assumes that the input file is in a specific format that includes general information,
        job durations, precedence relations, and resource availability.
    """
    
    """Processes the selected file and solves the jobshop problem."""
    # Load dataset
    parser = DatasetParser()
    data = parser.parse_dataset(file_path)

    general_info = data['general_info']
    duration_resources = data['duration_resources']
    precedence_relations = data['precedence_relations']
    resource_availability = data['resource_availability']

    # Extract job durations, precedence relations, and resource availability
    job_durations = {item["jobnr"] - 1: item["duration"] for item in duration_resources}
    resources = [key for key in duration_resources[0] if key.startswith("R")]
    resource_requirements = {item["jobnr"] - 1: {resource: item.get(resource, 0) for resource in resources} for item in duration_resources}
    precedence_dict = {item["jobnr"] - 1: [suc - 1 for suc in item["successors_list"]] for item in precedence_relations}
    resource_avail = {item["resource"]: item["qty"] for item in resource_availability}
    horizon = general_info["horizon"]

    # Create the model.
    model = cp_model.CpModel()

    # Create decision variables for start times and end times.
    start_vars = {}
    end_vars = {}
    intervals = {}
    for job_id, duration in job_durations.items():
        start_vars[job_id] = model.NewIntVar(0, horizon, f'start_{job_id}')
        end_vars[job_id] = model.NewIntVar(0, horizon, f'end_{job_id}')
        intervals[job_id] = model.NewIntervalVar(start_vars[job_id], duration, end_vars[job_id], f'interval_{job_id}')

    # Objective function: minimize the makespan.
    makespan = model.NewIntVar(0, horizon, 'makespan')
    model.AddMaxEquality(makespan, [end_vars[job_id] for job_id in job_durations])
    model.Minimize(makespan)

    # Add precedence constraints.
    for job_id, successors in precedence_dict.items():
        for successor in successors:
            model.Add(start_vars[successor] >= end_vars[job_id])

    # Add resource constraints.
    all_tasks = list(job_durations.keys())
    for resource, qty in resource_avail.items():
        demands = [resource_requirements[job_id].get(resource, 0) for job_id in all_tasks]
        if any(demands):
            model.AddCumulative([intervals[job_id] for job_id in all_tasks], demands, qty)

    # Solve the problem.
    solver = cp_model.CpSolver()
    status = solver.Solve(model)

    # Check if a solution was found.
    if status == cp_model.OPTIMAL or status == cp_model.FEASIBLE:
        print("Solution:")
        assigned_jobs = collections.defaultdict(list)
        for job_id in job_durations:
            start = solver.Value(start_vars[job_id])
            end = solver.Value(end_vars[job_id])

            # Extract resource usage information
            resource_usage = {resource: resource_requirements[job_id][resource] for resource in resource_requirements[job_id] if resource_requirements[job_id][resource] > 0}

            # Assign jobs with resource usage details
            assigned_jobs['jobs'].append((start, job_id, end - start, resource_usage))

        # Create output for each resource.
        output = ""
        for resource, tasks in assigned_jobs.items():
            # Sort tasks by starting time.
            tasks.sort()
            output += f"Resource {resource}:\n"
            for start, job_id, duration, resource_usage in tasks:
                name = f"job_{job_id + 1}"
                sol_tmp = f"  {name:<15} [{start},{start + duration}]"
                output += f"{sol_tmp}\n"
            output += "\n"  # Add a blank line between different resources

        # Print the solution found.
        print(f"Optimal Schedule Length: {solver.Value(makespan)}")
        print(output)

        # Create Gantt Chart
        create_gantt_chart(assigned_jobs, output_path='gantt_chart.png')

    else:
        print("No solution found.")

    # Print statistics about the solving process.
    print("\nStatistics")
    print(f"  - Status: {solver.StatusName(status)}")
