#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <omp.h>
#include "../../Common/jobshop_common.h"

#define MAX_STACK_SIZE 1000
#define MAX_SCHEDULE_ENTRIES (JMAX * OPMAX)

// Simplified Branch and Bound Node Structure
typedef struct {
    int job_progress[JMAX];    // Next operation for each job
    int machine_time[MMAX];    // Current completion time for each machine
    int lower_bound;           // Lower bound for this node
    int depth;                 // Number of operations scheduled
} BBNode;

typedef struct {
    int job;
    int op;
    int machine;
    int start_time;
    int duration;
} ScheduleEntry;

// Global variables
Shop global_shop;
int best_makespan = INT_MAX;
ScheduleEntry best_schedule[MAX_SCHEDULE_ENTRIES];
int best_schedule_len = 0;

// Initialize a node with empty schedule
void initialize_node(BBNode* node) {
    for (int j = 0; j < JMAX; j++) {
        node->job_progress[j] = 0;
    }
    for (int m = 0; m < MMAX; m++) {
        node->machine_time[m] = 0;
    }
    node->lower_bound = 0;
    node->depth = 0;
}

// Calculate lower bound using critical path analysis
int calculate_lower_bound(BBNode* node) {
    int max_bound = 0;
    // Job-based lower bound
    for (int j = 0; j < global_shop.njobs; j++) {
        int remaining_time = 0;
        for (int op = node->job_progress[j]; op < global_shop.nops; op++) {
            remaining_time += global_shop.plan[j][op].len;
        }
        if (remaining_time > max_bound) max_bound = remaining_time;
    }
    // Machine-based lower bound
    for (int m = 0; m < global_shop.nmachs; m++) {
        int machine_load = node->machine_time[m];
        for (int j = 0; j < global_shop.njobs; j++) {
            for (int op = node->job_progress[j]; op < global_shop.nops; op++) {
                if (global_shop.plan[j][op].mach == m + 1) {
                    machine_load += global_shop.plan[j][op].len;
                }
            }
        }
        if (machine_load > max_bound) max_bound = machine_load;
    }
    return max_bound;
}

int is_complete(BBNode* node) {
    for (int j = 0; j < global_shop.njobs; j++) {
        if (node->job_progress[j] < global_shop.nops) {
            return 0;
        }
    }
    return 1;
}

int calculate_makespan(BBNode* node) {
    int makespan = 0;
    for (int m = 0; m < global_shop.nmachs; m++) {
        if (node->machine_time[m] > makespan) {
            makespan = node->machine_time[m];
        }
    }
    return makespan;
}

// Parallel B&B: Each thread explores a different first-level child node
void expand_and_solve_parallel(BBNode* root, int num_threads) {
    BBNode children[JMAX];
    int child_count = 0;
    int job_indices[JMAX];
    // Generate all possible first-level children
    for (int j = 0; j < global_shop.njobs; j++) {
        int next_op = root->job_progress[j];
        if (next_op < global_shop.nops) {
            BBNode child = *root;
            int machine = global_shop.plan[j][next_op].mach - 1;
            int duration = global_shop.plan[j][next_op].len;
            int earliest_start = child.machine_time[machine];
            if (next_op > 0) {
                int prev_machine = global_shop.plan[j][next_op-1].mach - 1;
                if (child.machine_time[prev_machine] > earliest_start) {
                    earliest_start = child.machine_time[prev_machine];
                }
            }
            child.job_progress[j]++;
            child.machine_time[machine] = earliest_start + duration;
            child.depth++;
            child.lower_bound = calculate_lower_bound(&child);
            if (child.lower_bound < best_makespan) {
                children[child_count] = child;
                job_indices[child_count] = j;
                child_count++;
            }
        }
    }
    // printf("[DEBUG] First-level children generated: %d\n", child_count);
    if (child_count == 0) {
        // printf("[ERROR] No valid first-level children to explore. Exiting.\n");
        return;
    }
    // Parallel region: each thread explores a subtree
    #define MAX_NODES_EXPLORED 10000
    #pragma omp parallel for num_threads(num_threads) schedule(dynamic)
    for (int i = 0; i < child_count; i++) {
        BBNode* node_stack = (BBNode*)malloc(MAX_STACK_SIZE * sizeof(BBNode));
        int* schedule_len_stack = (int*)malloc(MAX_STACK_SIZE * sizeof(int));
        if (!node_stack || !schedule_len_stack) {
            free(node_stack); free(schedule_len_stack);
            continue;
        }
        ScheduleEntry* local_schedule = (ScheduleEntry*)malloc(MAX_SCHEDULE_ENTRIES * sizeof(ScheduleEntry));
        ScheduleEntry* local_best_schedule = (ScheduleEntry*)malloc(MAX_SCHEDULE_ENTRIES * sizeof(ScheduleEntry));
        if (!local_schedule || !local_best_schedule) {
            free(node_stack); free(schedule_len_stack); free(local_schedule); free(local_best_schedule);
            continue;
        }
        int stack_top = 0;
        int local_schedule_len = 0;
        int local_best_makespan = INT_MAX;
        int local_best_schedule_len = 0;
        int nodes_explored = 0;
        node_stack[stack_top] = children[i];
        schedule_len_stack[stack_top++] = local_schedule_len;
        // Defensive: check job_indices[i] is valid
        int opidx = root->job_progress[job_indices[i]];
        local_schedule[local_schedule_len].job = job_indices[i];
        local_schedule[local_schedule_len].op = opidx;
        local_schedule[local_schedule_len].machine = global_shop.plan[job_indices[i]][opidx].mach - 1;
        local_schedule[local_schedule_len].start_time = children[i].machine_time[local_schedule[local_schedule_len].machine] - global_shop.plan[job_indices[i]][opidx].len;
        local_schedule[local_schedule_len].duration = global_shop.plan[job_indices[i]][opidx].len;
        local_schedule_len++;
        while (stack_top > 0 && nodes_explored < MAX_NODES_EXPLORED) {
            BBNode current = node_stack[--stack_top];
            local_schedule_len = schedule_len_stack[stack_top];
            nodes_explored++;
            // Check if complete
            if (is_complete(&current)) {
                int makespan = calculate_makespan(&current);
                // printf("[DEBUG][OMP][Thread %d] Found complete solution with makespan %d\n", omp_get_thread_num(), makespan);
                if (makespan < local_best_makespan) {
                    local_best_makespan = makespan;
                    memcpy(local_best_schedule, local_schedule, sizeof(ScheduleEntry) * local_schedule_len);
                    local_best_schedule_len = local_schedule_len;
                }
                continue;
            }
            // Prune if lower bound exceeds current best
            if (current.lower_bound >= local_best_makespan) {
                // printf("[DEBUG][OMP][Thread %d] Pruned node at depth %d with lower_bound %d (local best %d)\n", omp_get_thread_num(), current.depth, current.lower_bound, local_best_makespan);
                continue;
            }
            // Expand node
            for (int j = 0; j < global_shop.njobs; j++) {
                int next_op = current.job_progress[j];
                if (next_op < global_shop.nops) {
                    BBNode child = current;
                    int machine = global_shop.plan[j][next_op].mach - 1;
                    int duration = global_shop.plan[j][next_op].len;
                    int earliest_start = child.machine_time[machine];
                    if (next_op > 0) {
                        int prev_machine = global_shop.plan[j][next_op-1].mach - 1;
                        if (child.machine_time[prev_machine] > earliest_start) {
                            earliest_start = child.machine_time[prev_machine];
                        }
                    }
                    child.job_progress[j]++;
                    child.machine_time[machine] = earliest_start + duration;
                    child.depth++;
                    child.lower_bound = calculate_lower_bound(&child);
                    if (stack_top < MAX_STACK_SIZE - 1 && child.lower_bound < local_best_makespan) {
                        node_stack[stack_top] = child;
                        memcpy(&local_schedule[local_schedule_len], &(ScheduleEntry){j, next_op, machine, earliest_start, duration}, sizeof(ScheduleEntry));
                        schedule_len_stack[stack_top++] = local_schedule_len + 1;
                    }
                }
            }
        }
        printf("[OMP][Thread %d] Explored %d nodes. Local best makespan: %d\n", omp_get_thread_num(), nodes_explored, local_best_makespan);
        // Update global best (critical section)
        #pragma omp critical
        {
            if (local_best_makespan < best_makespan) {
                best_makespan = local_best_makespan;
                memcpy(best_schedule, local_best_schedule, sizeof(ScheduleEntry) * local_best_schedule_len);
                best_schedule_len = local_best_schedule_len;
                // printf("[OMP] New best makespan found: %d\n", best_makespan);
            }
        }
        free(node_stack);
        free(schedule_len_stack);
        free(local_schedule);
        free(local_best_schedule);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: %s <input_file> <output_file> <num_threads>\n", argv[0]);
        return 1;
    }
    const char* input_file = argv[1];
    const char* output_file = argv[2];
    int num_threads = atoi(argv[3]);
    if (num_threads <= 0) num_threads = 1;
    // Load problem
    if (!load_problem_seq(input_file, &global_shop)) {
        printf("Error loading input file: %s\n", input_file);
        return 1;
    }
    // printf("Loaded problem: %d jobs, %d machines, %d operations per job\n", global_shop.njobs, global_shop.nmachs, global_shop.nops);
    char *basename = extract_basename(input_file);
    // printf("Starting OpenMP Parallel Branch & Bound for %s with %d threads...\n", basename ? basename : "unknown", num_threads);
    clock_t start_time = clock();
    BBNode root;
    initialize_node(&root);
    root.lower_bound = calculate_lower_bound(&root);
    expand_and_solve_parallel(&root, num_threads);
    clock_t end_time = clock();
    double execution_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    // printf("OpenMP Branch & Bound finished for %s.\n", basename ? basename : "unknown");
    // printf("Best makespan found: %d\n", best_makespan);
    // printf("Time taken: %.6f seconds\n", execution_time);
    // Save result
    FILE* file = fopen(output_file, "w");
    if (file) {
        // Output makespan
        fprintf(file, "%d\n", best_makespan);
        // Output per-job operation start times (as in Annex II)
        for (int j = 0; j < global_shop.njobs; j++) {
            for (int op = 0; op < global_shop.nops; op++) {
                // Find the entry for this job/op
                int found = 0;
                for (int k = 0; k < best_schedule_len; k++) {
                    if (best_schedule[k].job == j && best_schedule[k].op == op) {
                        fprintf(file, "%d ", best_schedule[k].start_time);
                        found = 1;
                        break;
                    }
                }
                if (!found) fprintf(file, "-1 "); // Not scheduled (should not happen)
            }
            fprintf(file, "\n");
        }
        fclose(file);
        printf("Results saved to %s\n", output_file);
    } else {
        printf("Error: Could not open output file %s for writing.\n", output_file);
    }
    if (basename) free(basename);
    return 0;
}
