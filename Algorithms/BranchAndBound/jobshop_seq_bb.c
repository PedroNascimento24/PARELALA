#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
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
typedef struct {
    BBNode node;
    ScheduleEntry schedule[MAX_SCHEDULE_ENTRIES];
    int schedule_len;
} StackEntry;

StackEntry node_stack[MAX_STACK_SIZE];
int stack_top = 0;

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
    
    // Job-based lower bound: remaining processing time for each job
    for (int j = 0; j < global_shop.njobs; j++) {
        int remaining_time = 0;
        for (int op = node->job_progress[j]; op < global_shop.nops; op++) {
            remaining_time += global_shop.plan[j][op].len;
        }
        if (remaining_time > max_bound) max_bound = remaining_time;
    }
    
    // Machine-based lower bound: current machine load + remaining work
    for (int m = 0; m < global_shop.nmachs; m++) {
        int machine_load = node->machine_time[m];
        for (int j = 0; j < global_shop.njobs; j++) {
            for (int op = node->job_progress[j]; op < global_shop.nops; op++) {
                if (global_shop.plan[j][op].mach == m + 1) { // Machine numbers are 1-indexed
                    machine_load += global_shop.plan[j][op].len;
                }
            }
        }
        if (machine_load > max_bound) max_bound = machine_load;
    }
    
    return max_bound;
}

// Check if all jobs are complete
int is_complete(BBNode* node) {
    for (int j = 0; j < global_shop.njobs; j++) {
        if (node->job_progress[j] < global_shop.nops) {
            return 0;
        }
    }
    return 1;
}

// Calculate makespan for a complete schedule
int calculate_makespan(BBNode* node) {
    int makespan = 0;
    for (int m = 0; m < global_shop.nmachs; m++) {
        if (node->machine_time[m] > makespan) {
            makespan = node->machine_time[m];
        }
    }
    return makespan;
}

// Find next available operations and create child nodes
void expand_node(StackEntry* parent_entry) {
    for (int j = 0; j < global_shop.njobs; j++) {
        int next_op = parent_entry->node.job_progress[j];
        
        // Check if this job has more operations to schedule
        if (next_op < global_shop.nops) {
            // Create child entry
            StackEntry child_entry = *parent_entry;
            BBNode* child = &child_entry.node;
            
            int machine = global_shop.plan[j][next_op].mach - 1; // Convert to 0-indexed
            int duration = global_shop.plan[j][next_op].len;
            
            // Calculate earliest start time
            int earliest_start = child->machine_time[machine];
            
            // Consider job precedence constraint
            if (next_op > 0) {
                int prev_machine = global_shop.plan[j][next_op-1].mach - 1;
                if (child->machine_time[prev_machine] > earliest_start) {
                    earliest_start = child->machine_time[prev_machine];
                }
            }
            
            // Update child node
            child->job_progress[j]++;
            child->machine_time[machine] = earliest_start + duration;
            child->depth++;
            child->lower_bound = calculate_lower_bound(child);
            
            // Add to stack if there's space and it's promising
            if (stack_top < MAX_STACK_SIZE - 1 && child->lower_bound < best_makespan) {
                // Record the operation in the child's schedule
                if (child_entry.schedule_len < MAX_SCHEDULE_ENTRIES) {
                    child_entry.schedule[child_entry.schedule_len].job = j;
                    child_entry.schedule[child_entry.schedule_len].op = next_op;
                    child_entry.schedule[child_entry.schedule_len].machine = machine;
                    child_entry.schedule[child_entry.schedule_len].start_time = earliest_start;
                    child_entry.schedule[child_entry.schedule_len].duration = duration;
                    child_entry.schedule_len++;
                }
                node_stack[stack_top++] = child_entry;
            }
        }
    }
}

// Main Branch and Bound algorithm
int solve_branch_and_bound() {
    // Initialize root node
    BBNode root;
    initialize_node(&root);
    root.lower_bound = calculate_lower_bound(&root);
    
    // Add root to stack
    StackEntry root_entry;
    root_entry.node = root;
    root_entry.schedule_len = 0;
    node_stack[stack_top++] = root_entry;
    
    int nodes_explored = 0;
    
    while (stack_top > 0 && nodes_explored < 10000) { // Limit exploration for efficiency
        StackEntry current_entry = node_stack[--stack_top];
        BBNode* current = &current_entry.node;
        nodes_explored++;
        
        // Check if complete
        if (is_complete(current)) {
            int makespan = calculate_makespan(current);
            if (makespan < best_makespan) {
                best_makespan = makespan;
                memcpy(best_schedule, current_entry.schedule, sizeof(ScheduleEntry) * current_entry.schedule_len);
                best_schedule_len = current_entry.schedule_len;
                printf("New best makespan found: %d\n", best_makespan);
            }
            continue;
        }
        
        // Prune if lower bound exceeds current best
        if (current->lower_bound >= best_makespan) {
            continue;
        }
        
        // Expand node
        expand_node(&current_entry);
    }
    
    printf("Nodes explored: %d\n", nodes_explored);
    return best_makespan;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }
    
    const char* input_file = argv[1];
    const char* output_file = argv[2];
    
    // Load problem
    if (!load_problem_seq(input_file, &global_shop)) {
        printf("Error loading input file: %s\n", input_file);
        return 1;
    }
    
    printf("Loaded problem: %d jobs, %d machines, %d operations per job\n", 
           global_shop.njobs, global_shop.nmachs, global_shop.nops);
    
    char *basename = extract_basename(input_file);
    printf("Starting Sequential Branch & Bound for %s...\n", basename ? basename : "unknown");
    
    // Measure execution time
    clock_t start_time = clock();
    
    int makespan = solve_branch_and_bound();
    
    clock_t end_time = clock();
    double execution_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    printf("Sequential Branch & Bound finished for %s.\n", basename ? basename : "unknown");
    printf("Best makespan found: %d\n", makespan);
    printf("Time taken: %.6f seconds\n", execution_time);
    
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
    }
    
    if (basename) free(basename);
    return 0;
}
