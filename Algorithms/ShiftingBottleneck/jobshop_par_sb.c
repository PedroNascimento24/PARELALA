// jobshop_par_sb.c
// Parallel job shop scheduler using Shifting Bottleneck heuristic (OpenMP)

#include "../../Common/jobshop_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h> // For mkdir on POSIX
#include <omp.h>      // For OpenMP

#define MAX_TOTAL_OPS (JMAX * OPMAX)
#define MAX_GRAPH_NODES (MAX_TOTAL_OPS + 2) // +2 for Source and Sink

// Adjacency list representation for the graph
typedef struct AdjListNode {
    int dest;
    struct AdjListNode* next;
} AdjListNode;

typedef struct {
    AdjListNode *head;
} AdjList;

// Global static graph representations to avoid large stack allocations
// These are modified sequentially or carefully, so global is okay for SB structure.
static AdjList adj[MAX_GRAPH_NODES];
static AdjList rev_adj[MAX_GRAPH_NODES]; 
static int node_proc_times[MAX_GRAPH_NODES]; 
static int est[MAX_GRAPH_NODES];      
static int tail_q[MAX_GRAPH_NODES];   


// Helper to add an edge to an adjacency list
void add_graph_edge(AdjList list_arr[], int src, int dest) {
    AdjListNode* newNode = (AdjListNode*)malloc(sizeof(AdjListNode));
    if (!newNode) {
        fprintf(stderr, "Memory allocation failed for graph edge.\n");
        exit(EXIT_FAILURE);
    }
    newNode->dest = dest;
    newNode->next = list_arr[src].head;
    list_arr[src].head = newNode;
}

// Helper to free an adjacency list
void free_graph_adj_list(AdjList list_arr[], int num_nodes) {
    for (int i = 0; i < num_nodes; ++i) {
        AdjListNode* current = list_arr[i].head;
        while (current != NULL) {
            AdjListNode* temp = current;
            current = current->next;
            free(temp);
        }
        list_arr[i].head = NULL;
    }
}

// Helper to convert (job, op_idx_in_job) to a graph node index
int op_to_node_idx(int job_idx, int op_idx_in_job, int ops_per_job_param) {
    return 1 + job_idx * ops_per_job_param + op_idx_in_job;
}

// Calculate Earliest Start Times (EST) for all nodes in a DAG
void calculate_est_AON(int source_node_idx, int num_total_nodes, AdjList current_adj[], 
                       int current_node_proc_times[], int result_est[]) {
    for (int i = 0; i < num_total_nodes; i++) {
        result_est[i] = 0; 
    }

    int* in_degree = (int*)calloc(num_total_nodes, sizeof(int));
    if (!in_degree) { fprintf(stderr, "Malloc failed for in_degree\n"); exit(EXIT_FAILURE); }

    for (int u = 0; u < num_total_nodes; ++u) {
        AdjListNode* p_crawl = current_adj[u].head;
        while (p_crawl) {
            in_degree[p_crawl->dest]++;
            p_crawl = p_crawl->next;
        }
    }

    int* queue = (int*)malloc(num_total_nodes * sizeof(int));
    if (!queue) { fprintf(stderr, "Malloc failed for queue\n"); free(in_degree); exit(EXIT_FAILURE); }
    int head = 0, tail_idx = 0; // Renamed tail to tail_idx to avoid conflict with global tail_q

    for (int i = 0; i < num_total_nodes; ++i) {
        if (in_degree[i] == 0) {
            queue[tail_idx++] = i;
        }
    }
    
    result_est[source_node_idx] = 0;

    int count_processed_nodes = 0;
    while (head < tail_idx) {
        int u = queue[head++];
        count_processed_nodes++;

        AdjListNode* p_crawl = current_adj[u].head;
        while (p_crawl) {
            int v = p_crawl->dest;
            if (result_est[v] < result_est[u] + current_node_proc_times[u]) {
                result_est[v] = result_est[u] + current_node_proc_times[u];
            }
            in_degree[v]--;
            if (in_degree[v] == 0) {
                queue[tail_idx++] = v;
            }
            p_crawl = p_crawl->next;
        }
    }
    free(in_degree);
    free(queue);
}


// Main Shifting Bottleneck scheduling logic - Parallel Version
void shifting_bottleneck_schedule(Shop *shop, int num_threads) {
    omp_set_num_threads(num_threads);

    int njobs = shop->njobs;
    int nops_per_job = shop->nops; 
    
    if (njobs == 0 || nops_per_job == 0) {
        fprintf(stderr, "No jobs or operations to schedule.\n");
        return;
    }
    if (njobs > JMAX || nops_per_job > OPMAX) {
        fprintf(stderr, "Problem size exceeds defined limits (JMAX/OPMAX).\n");
        return;
    }

    int num_ops_total = njobs * nops_per_job;
    int source_node = 0;
    int sink_node = num_ops_total + 1;
    int num_graph_nodes = num_ops_total + 2;

    if (num_graph_nodes > MAX_GRAPH_NODES) {
        fprintf(stderr, "Calculated graph nodes %d exceeds MAX_GRAPH_NODES %d\n", num_graph_nodes, MAX_GRAPH_NODES);
        return;
    }

    for(int i=0; i<num_graph_nodes; ++i) {
        adj[i].head = NULL;
        rev_adj[i].head = NULL;
        node_proc_times[i] = 0; 
        est[i] = 0;
        tail_q[i] = 0;
    }

    node_proc_times[source_node] = 0;
    node_proc_times[sink_node] = 0;  

    for (int j = 0; j < njobs; ++j) {
        for (int o = 0; o < nops_per_job; ++o) {
            int current_op_node = op_to_node_idx(j, o, nops_per_job);
            if(current_op_node < 1 || current_op_node > num_ops_total) continue; 

            node_proc_times[current_op_node] = shop->plan[j][o].len;

            if (o == 0) { 
                add_graph_edge(adj, source_node, current_op_node);
            }
            if (o < nops_per_job - 1) { 
                int next_op_node = op_to_node_idx(j, o + 1, nops_per_job);
                if(next_op_node < 1 || next_op_node > num_ops_total) continue;
                add_graph_edge(adj, current_op_node, next_op_node);
            }
            if (o == nops_per_job - 1) { 
                add_graph_edge(adj, current_op_node, sink_node);
            }
        }
    }

    int sequenced_machines_flags[MMAX];
    for(int i=0; i<shop->nmachs; ++i) sequenced_machines_flags[i] = 0;
    int num_sequenced_machines_count = 0;

    int* best_sequence_for_bottleneck_machine_global = (int*)malloc(JMAX * sizeof(int));
    if (!best_sequence_for_bottleneck_machine_global) {
        fprintf(stderr, "Failed to allocate memory for SB global best sequence buffer\n");
        free_graph_adj_list(adj, num_graph_nodes);
        return;
    }
    
    int* temp_best_sequence_storage = (int*)malloc(JMAX * sizeof(int)); // Used in the reduction part
     if (!temp_best_sequence_storage) {
         fprintf(stderr, "Failed to allocate temp_best_sequence_storage\n");
         free(best_sequence_for_bottleneck_machine_global);
         free_graph_adj_list(adj, num_graph_nodes);
         return;
    }


    while (num_sequenced_machines_count < shop->nmachs) {
        calculate_est_AON(source_node, num_graph_nodes, adj, node_proc_times, est);
        
        free_graph_adj_list(rev_adj, num_graph_nodes); 
        for(int u_node=0; u_node<num_graph_nodes; ++u_node) {
            AdjListNode* p_crawl = adj[u_node].head;
            while(p_crawl){
                add_graph_edge(rev_adj, p_crawl->dest, u_node); 
                p_crawl = p_crawl->next;
            }
        }
        calculate_est_AON(sink_node, num_graph_nodes, rev_adj, node_proc_times, tail_q);

        int overall_best_machine_idx = -1; 
        long long overall_max_bottleneck_metric = -1; 
        int overall_best_seq_len = 0;
        
        // Initialize temp_best_sequence_storage for this iteration if needed, or rely on overwrite
        // For safety, ensure it's clean or correctly populated by the critical section logic.

        #pragma omp parallel
        {
            int local_best_machine_idx = -1;
            long long local_max_bottleneck_metric = -1;
            int local_best_seq_len = 0;
            
            OneMachineOpInfo* thread_local_machine_ops_buffer = (OneMachineOpInfo*)malloc(JMAX * sizeof(OneMachineOpInfo));
            int* thread_local_current_sequence_nodes_buffer = (int*)malloc(JMAX * sizeof(int));
            int* thread_local_best_sequence_for_machine = (int*)malloc(JMAX * sizeof(int));

            if (!thread_local_machine_ops_buffer || !thread_local_current_sequence_nodes_buffer || !thread_local_best_sequence_for_machine) {
                // fprintf(stderr, "Thread %d: Failed to allocate local buffers\n", omp_get_thread_num());
                // This thread cannot participate effectively.
            } else {
                #pragma omp for schedule(dynamic)
                for (int m_idx = 0; m_idx < shop->nmachs; ++m_idx) {
                    if (sequenced_machines_flags[m_idx]) continue;

                    int num_ops_on_this_machine = 0;
                    int stop_collecting_for_this_machine = 0;
                    for (int j = 0; j < njobs; ++j) {
                        for (int o = 0; o < nops_per_job; ++o) {
                            if (shop->plan[j][o].mach == m_idx + 1) {
                                if (num_ops_on_this_machine >= JMAX) {
                                    stop_collecting_for_this_machine = 1; break;
                                }
                                int op_node = op_to_node_idx(j, o, nops_per_job);
                                if(op_node < 1 || op_node > num_ops_total) continue;

                                thread_local_machine_ops_buffer[num_ops_on_this_machine].op_node_id = op_node;
                                thread_local_machine_ops_buffer[num_ops_on_this_machine].p_time = node_proc_times[op_node];
                                thread_local_machine_ops_buffer[num_ops_on_this_machine].r_time = est[op_node];
                                thread_local_machine_ops_buffer[num_ops_on_this_machine].q_time_val = tail_q[op_node]; 
                                num_ops_on_this_machine++;
                            }
                        }
                        if (stop_collecting_for_this_machine) break;
                    }

                    if (num_ops_on_this_machine == 0) continue;

                    for(int i=0; i<num_ops_on_this_machine-1; ++i) {
                        for(int k=0; k<num_ops_on_this_machine-i-1; ++k) {
                            if(thread_local_machine_ops_buffer[k].r_time > thread_local_machine_ops_buffer[k+1].r_time ||
                               (thread_local_machine_ops_buffer[k].r_time == thread_local_machine_ops_buffer[k+1].r_time && thread_local_machine_ops_buffer[k].p_time > thread_local_machine_ops_buffer[k+1].p_time)) {
                                OneMachineOpInfo temp_op_info = thread_local_machine_ops_buffer[k];
                                thread_local_machine_ops_buffer[k] = thread_local_machine_ops_buffer[k+1];
                                thread_local_machine_ops_buffer[k+1] = temp_op_info;
                            }
                        }
                    }
                    
                    long long current_machine_completion_time = 0;
                    long long current_machine_cmax_val = 0;
                    for (int i = 0; i < num_ops_on_this_machine; ++i) {
                        current_machine_completion_time = (current_machine_completion_time > thread_local_machine_ops_buffer[i].r_time) ? current_machine_completion_time : thread_local_machine_ops_buffer[i].r_time;
                        current_machine_completion_time += thread_local_machine_ops_buffer[i].p_time;
                        if (current_machine_completion_time > current_machine_cmax_val) {
                            current_machine_cmax_val = current_machine_completion_time;
                        }
                        thread_local_current_sequence_nodes_buffer[i] = thread_local_machine_ops_buffer[i].op_node_id;
                    }
                    
                    long long current_bottleneck_metric = current_machine_cmax_val; 

                    if (current_bottleneck_metric > local_max_bottleneck_metric) {
                        local_max_bottleneck_metric = current_bottleneck_metric;
                        local_best_machine_idx = m_idx;
                        local_best_seq_len = num_ops_on_this_machine;
                        for(int i=0; i<num_ops_on_this_machine; ++i) {
                            thread_local_best_sequence_for_machine[i] = thread_local_current_sequence_nodes_buffer[i];
                        }
                    }
                } 
            
                #pragma omp critical
                {
                    if (local_best_machine_idx != -1 && local_max_bottleneck_metric > overall_max_bottleneck_metric) {
                        overall_max_bottleneck_metric = local_max_bottleneck_metric;
                        overall_best_machine_idx = local_best_machine_idx;
                        overall_best_seq_len = local_best_seq_len;
                        for(int i=0; i<local_best_seq_len; ++i) {
                            temp_best_sequence_storage[i] = thread_local_best_sequence_for_machine[i];
                        }
                    }
                }
            } 

            if(thread_local_machine_ops_buffer) free(thread_local_machine_ops_buffer);
            if(thread_local_current_sequence_nodes_buffer) free(thread_local_current_sequence_nodes_buffer);
            if(thread_local_best_sequence_for_machine) free(thread_local_best_sequence_for_machine);
        } 

        if (overall_best_machine_idx == -1) {
            break; 
        }
        
        for(int i=0; i<overall_best_seq_len; ++i) {
            best_sequence_for_bottleneck_machine_global[i] = temp_best_sequence_storage[i];
        }

        for (int i = 0; i < overall_best_seq_len - 1; ++i) {
            int u_node = best_sequence_for_bottleneck_machine_global[i];
            int v_node = best_sequence_for_bottleneck_machine_global[i+1];
            if (u_node < 1 || u_node > num_ops_total || v_node < 1 || v_node > num_ops_total) continue;
            add_graph_edge(adj, u_node, v_node);
        }
        sequenced_machines_flags[overall_best_machine_idx] = 1;
        num_sequenced_machines_count++;
        
        // Reset for next iteration's parallel reduction (overall_best_machine_idx is key)
        // overall_max_bottleneck_metric and overall_best_seq_len will be overwritten if a better one is found.
    } 

    free(temp_best_sequence_storage);    calculate_est_AON(source_node, num_graph_nodes, adj, node_proc_times, est);

    // Apply resource-aware scheduling to ensure no machine overlaps
    int machine_available_time[MMAX];
    for(int m = 0; m < shop->nmachs; m++) {
        machine_available_time[m] = 0;
    }

    // Create a list of all operations sorted by their EST
    typedef struct {
        int job;
        int op;
        int est_time;
        int machine;
        int duration;
    } OpScheduleInfo;
    
    OpScheduleInfo* op_list = (OpScheduleInfo*)malloc(num_ops_total * sizeof(OpScheduleInfo));
    if (!op_list) {
        fprintf(stderr, "Failed to allocate memory for operation list\n");
        free_graph_adj_list(adj, num_graph_nodes);
        return;
    }
    
    int op_count = 0;
    for (int j = 0; j < njobs; ++j) {
        for (int o = 0; o < nops_per_job; ++o) {
            int op_node = op_to_node_idx(j, o, nops_per_job);
            if(op_node < 1 || op_node > num_ops_total) continue;
            
            op_list[op_count].job = j;
            op_list[op_count].op = o;
            op_list[op_count].est_time = est[op_node];
            op_list[op_count].machine = shop->plan[j][o].mach - 1; // Convert to 0-indexed
            op_list[op_count].duration = shop->plan[j][o].len;
            op_count++;
        }
    }
    
    // Sort operations by EST, then by job, then by operation
    for(int i = 0; i < op_count - 1; i++) {
        for(int k = 0; k < op_count - i - 1; k++) {
            if(op_list[k].est_time > op_list[k+1].est_time ||
               (op_list[k].est_time == op_list[k+1].est_time && op_list[k].job > op_list[k+1].job) ||
               (op_list[k].est_time == op_list[k+1].est_time && op_list[k].job == op_list[k+1].job && op_list[k].op > op_list[k+1].op)) {
                OpScheduleInfo temp = op_list[k];
                op_list[k] = op_list[k+1];
                op_list[k+1] = temp;
            }
        }
    }
    
    // Schedule operations ensuring no machine conflicts
    for(int i = 0; i < op_count; i++) {
        int j = op_list[i].job;
        int o = op_list[i].op;
        int machine_idx = op_list[i].machine;
        int duration = op_list[i].duration;
        int earliest_start = op_list[i].est_time;
        
        // Check job precedence constraint
        if(o > 0) {
            int prev_end_time = shop->plan[j][o-1].stime + shop->plan[j][o-1].len;
            if(earliest_start < prev_end_time) {
                earliest_start = prev_end_time;
            }
        }
        
        // Check machine availability constraint
        if(earliest_start < machine_available_time[machine_idx]) {
            earliest_start = machine_available_time[machine_idx];
        }
        
        // Schedule the operation
        shop->plan[j][o].stime = earliest_start;
        machine_available_time[machine_idx] = earliest_start + duration;
    }
    
    free(op_list);
    
    free_graph_adj_list(adj, num_graph_nodes);
    free_graph_adj_list(rev_adj, num_graph_nodes);
    free(best_sequence_for_bottleneck_machine_global);
}

int main(int argc, char *argv[]) {
    if (argc < 4) { // Expect input_file, output_file, num_threads
        fprintf(stderr, "Usage: %s <input_file> <output_file> <num_threads>\n", argv[0]);
        return 1;
    }
    char *input_file = argv[1];
    char *output_file = argv[2];
    int num_threads = atoi(argv[3]);

    if (num_threads <= 0) {
        fprintf(stderr, "Number of threads must be positive.\n");
        return 1;
    }

    Shop shop_instance;
    Shop* shop = &shop_instance; // Use the standard Shop struct

    // if (!read_problem(shop, input_file)) { // Use the common read_problem
    if (!load_problem_seq(input_file, shop)) { // Use load_problem_seq
        fprintf(stderr, "Error loading problem from %s\n", input_file);
        return 1;
    }
    
    if (shop->njobs == 0 || shop->nops == 0) {
        printf("No jobs or operations found in the input file.\n");
        // write_solution(shop, output_file, 0); // Write empty solution if needed
        return 0;
    }

    double start_time = omp_get_wtime();

    shifting_bottleneck_schedule(shop, num_threads); // Call the parallel version

    double end_time = omp_get_wtime();
    double time_taken = end_time - start_time;

    int makespan = 0;
    for (int j = 0; j < shop->njobs; j++) {
        for (int o = 0; o < shop->nops; o++) {
            int end_op_time = shop->plan[j][o].stime + shop->plan[j][o].len;
            if (end_op_time > makespan) {
                makespan = end_op_time;
            }
        }
    }
    
    // Create directory for output file if it doesn't exist
    char *last_slash = strrchr(output_file, '/');
    #ifdef _WIN32
    char *last_backslash = strrchr(output_file, '\\');
    if (last_backslash > last_slash) last_slash = last_backslash;
    #endif

    if (last_slash != NULL) {
        char dir_path[256];
        strncpy(dir_path, output_file, last_slash - output_file);
        dir_path[last_slash - output_file] = '\0';
        
        struct stat st = {0};
        if (stat(dir_path, &st) == -1) {
            #ifdef _WIN32
                _mkdir(dir_path); // For Windows
            #else
                mkdir(dir_path, 0777); // For POSIX
            #endif
        }
    }

    // write_solution(shop, output_file, makespan); // Use the common write_solution
    save_result_seq(output_file, shop); // Use save_result_seq, makespan is calculated internally by save_result_seq if needed or not used.
                                        // The save_result_seq in common.c calculates makespan.

    printf("Makespan: %d\n", makespan);
    printf("Time taken: %f seconds\n", time_taken);
    fflush(stdout); // Ensure output is flushed, especially if redirecting

    return 0;
}

// Note: OneMachineOpInfo struct definition was missing from the provided snippet.
// Assuming it's defined as:
/*
typedef struct { 
    int job_idx; 
    int op_idx_in_job; 
    int op_node_id; 
    int r_time;  // Release time (EST)
    int p_time;  // Processing time
    int q_time_val; // Tail time (longest path to sink)
} OneMachineOpInfo;
*/
// This should ideally be defined globally or passed around if it's used by multiple functions.
// For this parallel version, it's used locally within the parallel region's scope.
// MOVED TO jobshop_common.h
