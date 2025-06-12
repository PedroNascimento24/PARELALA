#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <pthread.h>
#include "../../Common/jobshop_common.h"

#define MAX_STACK_SIZE 1000
#define MAX_THREADS 8

// Simplified Branch and Bound Node Structure
typedef struct {
    int job_progress[JMAX];    // Next operation for each job
    int machine_time[MMAX];    // Current completion time for each machine
    int lower_bound;           // Lower bound for this node
    int depth;                 // Number of operations scheduled
} BBNode;

// Thread data structure
typedef struct {
    int thread_id;
    BBNode* local_stack;
    int local_stack_top;
    int best_local_makespan;
    int nodes_explored;
} ThreadData;

// Global variables
ParallelShop global_shop;
int best_global_makespan = INT_MAX;
pthread_mutex_t best_makespan_mutex = PTHREAD_MUTEX_INITIALIZER;
BBNode shared_stack[MAX_STACK_SIZE];
int shared_stack_top = 0;
pthread_mutex_t stack_mutex = PTHREAD_MUTEX_INITIALIZER;

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

// Get work from shared stack (thread-safe)
int get_work(BBNode* node) {
    pthread_mutex_lock(&stack_mutex);
    if (shared_stack_top > 0) {
        *node = shared_stack[--shared_stack_top];
        pthread_mutex_unlock(&stack_mutex);
        return 1;
    }
    pthread_mutex_unlock(&stack_mutex);
    return 0;
}

// Add work to shared stack (thread-safe)
void add_work(BBNode* node) {
    pthread_mutex_lock(&stack_mutex);
    if (shared_stack_top < MAX_STACK_SIZE - 1) {
        shared_stack[shared_stack_top++] = *node;
    }
    pthread_mutex_unlock(&stack_mutex);
}

// Update global best makespan (thread-safe)
void update_best_makespan(int makespan) {
    pthread_mutex_lock(&best_makespan_mutex);
    if (makespan < best_global_makespan) {
        best_global_makespan = makespan;
        printf("Thread found new best makespan: %d\n", makespan);
    }
    pthread_mutex_unlock(&best_makespan_mutex);
}

// Get current best makespan (thread-safe)
int get_best_makespan() {
    pthread_mutex_lock(&best_makespan_mutex);
    int current_best = best_global_makespan;
    pthread_mutex_unlock(&best_makespan_mutex);
    return current_best;
}

// Worker thread function
void* worker_thread(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    BBNode current;
    
    data->best_local_makespan = INT_MAX;
    data->nodes_explored = 0;
    
    while (data->nodes_explored < 2000) { // Limit per thread
        // Try to get work
        if (!get_work(&current)) {
            break; // No more work
        }
        
        data->nodes_explored++;
        
        // Check if complete
        if (is_complete(&current)) {
            int makespan = calculate_makespan(&current);
            if (makespan < data->best_local_makespan) {
                data->best_local_makespan = makespan;
                update_best_makespan(makespan);
            }
            continue;
        }
        
        // Prune if lower bound exceeds current best
        if (current.lower_bound >= get_best_makespan()) {
            continue;
        }
        
        // Expand node: create children
        for (int j = 0; j < global_shop.njobs; j++) {
            int next_op = current.job_progress[j];
            
            if (next_op < global_shop.nops) {
                BBNode child = current;
                
                int machine = global_shop.plan[j][next_op].mach - 1;
                int duration = global_shop.plan[j][next_op].len;
                
                // Calculate earliest start time
                int earliest_start = child.machine_time[machine];
                
                // Consider job precedence
                if (next_op > 0) {
                    int prev_machine = global_shop.plan[j][next_op-1].mach - 1;
                    if (child.machine_time[prev_machine] > earliest_start) {
                        earliest_start = child.machine_time[prev_machine];
                    }
                }
                
                // Update child
                child.job_progress[j]++;
                child.machine_time[machine] = earliest_start + duration;
                child.depth++;
                child.lower_bound = calculate_lower_bound(&child);
                
                // Add promising children back to shared stack
                if (child.lower_bound < get_best_makespan()) {
                    add_work(&child);
                }
            }
        }
    }
    
    return NULL;
}

// Main parallel Branch and Bound algorithm
int solve_parallel_branch_and_bound(int num_threads) {
    // Initialize root node
    BBNode root;
    initialize_node(&root);
    root.lower_bound = calculate_lower_bound(&root);
    
    // Add root to shared stack
    shared_stack[shared_stack_top++] = root;
    
    // Create threads
    pthread_t threads[MAX_THREADS];
    ThreadData thread_data[MAX_THREADS];
    
    if (num_threads > MAX_THREADS) num_threads = MAX_THREADS;
    
    printf("Starting %d worker threads...\n", num_threads);
    
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].local_stack = malloc(MAX_STACK_SIZE * sizeof(BBNode));
        thread_data[i].local_stack_top = 0;
        pthread_create(&threads[i], NULL, worker_thread, &thread_data[i]);
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        printf("Thread %d explored %d nodes, best local makespan: %d\n", 
               i, thread_data[i].nodes_explored, thread_data[i].best_local_makespan);
        free(thread_data[i].local_stack);
    }
    
    return best_global_makespan;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Usage: %s <input_file> <output_file> <num_threads>\n", argv[0]);
        return 1;
    }
    
    const char* input_file = argv[1];
    const char* output_file = argv[2];
    int num_threads = atoi(argv[3]);
    
    if (num_threads <= 0 || num_threads > MAX_THREADS) {
        printf("Number of threads must be between 1 and %d\n", MAX_THREADS);
        return 1;
    }
    
    // Load problem
    if (!load_problem_par(input_file, &global_shop)) {
        printf("Error loading input file: %s\n", input_file);
        return 1;
    }
    
    printf("Loaded problem: %d jobs, %d machines, %d operations per job\n", 
           global_shop.njobs, global_shop.nmachs, global_shop.nops);
    
    char *basename = extract_basename(input_file);
    printf("Starting Parallel Branch & Bound for %s with %d threads...\n", 
           basename ? basename : "unknown", num_threads);
    
    // Measure execution time
    clock_t start_time = clock();
    
    int makespan = solve_parallel_branch_and_bound(num_threads);
    
    clock_t end_time = clock();
    double execution_time = ((double)(end_time - start_time)) / CLOCKS_PER_SEC;
    
    printf("Parallel Branch & Bound finished for %s.\n", basename ? basename : "unknown");
    printf("Best makespan found: %d\n", makespan);
    printf("Time taken: %.6f seconds\n", execution_time);
    
    // Save result
    FILE* file = fopen(output_file, "w");
    if (file) {
        fprintf(file, "Parallel Branch & Bound Results\n");
        fprintf(file, "Input file: %s\n", input_file);
        fprintf(file, "Jobs: %d, Machines: %d, Operations per job: %d\n", 
                global_shop.njobs, global_shop.nmachs, global_shop.nops);
        fprintf(file, "Number of threads: %d\n", num_threads);
        fprintf(file, "Best makespan: %d\n", makespan);
        fprintf(file, "Execution time: %.6f seconds\n", execution_time);
        fclose(file);
        printf("Results saved to %s\n", output_file);
    }
    
    if (basename) free(basename);
    return 0;
}
