import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from matplotlib.ticker import MaxNLocator, MultipleLocator

def create_gantt_chart(schedule, output_path='ganttchart.png'):
    """
    Creates a Gantt chart from the given job schedule and saves it as an image file.
    Parameters:
    schedule (dict): A dictionary containing job schedule information. 
                     The dictionary should have a key 'jobs' which is a list of tuples.
                     Each tuple should contain:
                     - start (int): The start time of the job.
                     - job_id (int): The unique identifier of the job.
                     - duration (int): The duration of the job.
                     - resource_usage (dict): A dictionary of resources used by the job with resource names as keys and quantities as values.
    output_path (str): The file path where the Gantt chart image will be saved. Default is 'ganttchart.png'.
    Returns:
    None
    The function generates a Gantt chart using matplotlib, displaying each job as a colored bar with its ID, time range, and resource usage.
    The chart is saved to the specified output path and displayed.
    """
    fig, ax = plt.subplots(figsize=(16, 9))
    colors = ['#ffcc00', '#e74c3c', '#3498db', '#2ecc71', '#9b59b6', '#f39c12', '#34495e', '#d35400', '#1abc9c']

    y_labels = []
    y_pos = 0
    color_index = 0
    job_height = 0.6  # Set a consistent height for all jobs
    job_gap = 0.1     # Gap between jobs in the same row

    for job_data in schedule['jobs']:
        start, job_id, duration, resource_usage = job_data
        # Extract resource information
        resource_text = ", ".join([f"{res}: {qty}" for res, qty in resource_usage.items()])

        # Debugging information
        print(f'Plotting Job {job_id}: start={start}, duration={duration} and Resource Usage: {resource_text}')
        
        # Ensure start and duration are correct
        assert duration >= 0, f"Duration cannot be negative: Job {job_id}"

        # Plot each job for the resource
        ax.broken_barh(
            [(start, duration)], 
            (y_pos, job_height),  # Consistent y position for each job
            facecolors=(colors[color_index % len(colors)]), 
            edgecolor='black', 
            linewidth=1.2, 
            alpha=0.85, 
            capstyle='round'
        )
        # Add text indicating the Job ID, time range, resource, and quantity used
        ax.text(
            start + duration / 2, 
            y_pos + (job_height / 2), 
            f'Job {job_id}\n[{start}, {start + duration}]\n{resource_text}', 
            ha='center', 
            va='center', 
            color='white', 
            fontsize=9, 
            weight='bold'
        )
        color_index += 1
        y_pos += job_height + job_gap  # Move to the next job

    ax.set_yticks([i * (job_height + job_gap) + (job_height / 2) for i in range(len(schedule['jobs']))])
    ax.set_yticklabels(y_labels, fontsize=12, weight='bold')
    ax.set_xlabel('Time', fontsize=14, weight='bold')
    ax.set_ylabel('Resources', fontsize=14, weight='bold')
    ax.set_title('Gantt Chart of Job Schedule', fontsize=18, weight='bold', pad=20)

    # Set the x-axis major locator to increments of 1 unit
    ax.xaxis.set_major_locator(MultipleLocator(1))

    plt.grid(axis='x', linestyle='--', linewidth=0.8, alpha=0.7)
    plt.tight_layout()
    plt.savefig(output_path)
    plt.show()
