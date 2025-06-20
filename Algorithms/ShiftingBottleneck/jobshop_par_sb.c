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

// Adjacency matrix representation for the graph
static int adj[MAX_GRAPH_NODES][MAX_GRAPH_NODES];
static int rev_adj[MAX_GRAPH_NODES][MAX_GRAPH_NODES];
static int node_proc_times[MAX_GRAPH_NODES];
static int est[MAX_GRAPH_NODES];
static int tail_q[MAX_GRAPH_NODES];

typedef int (*GraphMatrix)[MAX_GRAPH_NODES];
void add_graph_edge(GraphMatrix matrix, int src, int dest) {
    matrix[src][dest] = 1;
}
void clear_graph_matrix(GraphMatrix matrix, int num_nodes) {
    for (int i = 0; i < num_nodes; ++i) {
        for (int j = 0; j < num_nodes; ++j) {
            matrix[i][j] = 0;
        }
    }
}
int op_to_node_idx(int job_idx, int op_idx_in_job, int ops_per_job_param) {
    return 1 + job_idx * ops_per_job_param + op_idx_in_job;
}
void calculate_est_AON(int source_node_idx, int num_total_nodes, GraphMatrix matrix,
                       int current_node_proc_times[], int result_est[]) {
    for (int i = 0; i < num_total_nodes; i++) {
        result_est[i] = 0;
    }
    int in_degree[MAX_GRAPH_NODES] = {0};
    for (int u = 0; u < num_total_nodes; ++u) {
        for (int v = 0; v < num_total_nodes; ++v) {
            if (matrix[u][v]) in_degree[v]++;
        }
    }
    int queue[MAX_GRAPH_NODES];
    int head = 0, tail_idx = 0;
    for (int i = 0; i < num_total_nodes; ++i) {
        if (in_degree[i] == 0) {
            queue[tail_idx++] = i;
        }
    }
    result_est[source_node_idx] = 0;
    while (head < tail_idx) {
        int u = queue[head++];
        for (int v = 0; v < num_total_nodes; ++v) {
            if (matrix[u][v]) {
                if (result_est[v] < result_est[u] + current_node_proc_times[u]) {
                    result_est[v] = result_est[u] + current_node_proc_times[u];
                }
                in_degree[v]--;
                if (in_degree[v] == 0) {
                    queue[tail_idx++] = v;
                }
            }
        }
    }
}
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
    clear_graph_matrix(adj, num_graph_nodes);
    clear_graph_matrix(rev_adj, num_graph_nodes);
    for (int i = 0; i < num_graph_nodes; ++i) {
        node_proc_times[i] = 0;
        est[i] = 0;
        tail_q[i] = 0;
    }
    node_proc_times[source_node] = 0;
    node_proc_times[sink_node] = 0;
    for (int j = 0; j < njobs; ++j) {
        for (int o = 0; o < nops_per_job; ++o) {
            int current_op_node = op_to_node_idx(j, o, nops_per_job);
            if (current_op_node < 1 || current_op_node > num_ops_total) continue;
            node_proc_times[current_op_node] = shop->plan[j][o].len;
            if (o == 0) {
                add_graph_edge(adj, source_node, current_op_node);
            }
            if (o < nops_per_job - 1) {
                int next_op_node = op_to_node_idx(j, o + 1, nops_per_job);
                if (next_op_node < 1 || next_op_node > num_ops_total) continue;
                add_graph_edge(adj, current_op_node, next_op_node);
            }
            if (o == nops_per_job - 1) {
                add_graph_edge(adj, current_op_node, sink_node);
            }
        }
    }
    int sequenced_machines_flags[MMAX];
    for (int i = 0; i < shop->nmachs; ++i) sequenced_machines_flags[i] = 0;
    int num_sequenced_machines_count = 0;
    int best_sequence_for_bottleneck_machine_global[JMAX];
    int temp_best_sequence_storage[JMAX];
    while (num_sequenced_machines_count < shop->nmachs) {
        calculate_est_AON(source_node, num_graph_nodes, adj, node_proc_times, est);
        clear_graph_matrix(rev_adj, num_graph_nodes);
        for (int u_node = 0; u_node < num_graph_nodes; ++u_node) {
            for (int v_node = 0; v_node < num_graph_nodes; ++v_node) {
                if (adj[u_node][v_node]) {
                    add_graph_edge(rev_adj, v_node, u_node);
                }
            }
        }
        calculate_est_AON(sink_node, num_graph_nodes, rev_adj, node_proc_times, tail_q);
        int overall_best_machine_idx = -1;
        long long overall_max_bottleneck_metric = -1;
        int overall_best_seq_len = 0;
        #pragma omp parallel
        {
            int local_best_machine_idx = -1;
            long long local_max_bottleneck_metric = -1;
            int local_best_seq_len = 0;
            OneMachineOpInfo thread_local_machine_ops_buffer[JMAX];
            int thread_local_current_sequence_nodes_buffer[JMAX];
            int thread_local_best_sequence_for_machine[JMAX];
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
                            if (op_node < 1 || op_node > num_ops_total) continue;
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
                for (int i = 0; i < num_ops_on_this_machine - 1; ++i) {
                    for (int k = 0; k < num_ops_on_this_machine - i - 1; ++k) {
                        if (thread_local_machine_ops_buffer[k].r_time > thread_local_machine_ops_buffer[k + 1].r_time ||
                            (thread_local_machine_ops_buffer[k].r_time == thread_local_machine_ops_buffer[k + 1].r_time && thread_local_machine_ops_buffer[k].p_time > thread_local_machine_ops_buffer[k + 1].p_time)) {
                            OneMachineOpInfo temp_op_info = thread_local_machine_ops_buffer[k];
                            thread_local_machine_ops_buffer[k] = thread_local_machine_ops_buffer[k + 1];
                            thread_local_machine_ops_buffer[k + 1] = temp_op_info;
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
                    for (int i = 0; i < num_ops_on_this_machine; ++i) {
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
                    for (int i = 0; i < local_best_seq_len; ++i) {
                        temp_best_sequence_storage[i] = thread_local_best_sequence_for_machine[i];
                    }
                }
            }
        }
        if (overall_best_machine_idx == -1) {
            break;
        }
        for (int i = 0; i < overall_best_seq_len; ++i) {
            best_sequence_for_bottleneck_machine_global[i] = temp_best_sequence_storage[i];
        }
        for (int i = 0; i < overall_best_seq_len - 1; ++i) {
            int u_node = best_sequence_for_bottleneck_machine_global[i];
            int v_node = best_sequence_for_bottleneck_machine_global[i + 1];
            if (u_node < 1 || u_node > num_ops_total || v_node < 1 || v_node > num_ops_total) continue;
            add_graph_edge(adj, u_node, v_node);
        }
        sequenced_machines_flags[overall_best_machine_idx] = 1;
        num_sequenced_machines_count++;
    }
    calculate_est_AON(source_node, num_graph_nodes, adj, node_proc_times, est);
    int machine_available_time[MMAX];
    for (int m = 0; m < shop->nmachs; m++) {
        machine_available_time[m] = 0;
    }
    typedef struct {
        int job;
        int op;
        int est_time;
        int machine;
        int duration;
    } OpScheduleInfo;
    OpScheduleInfo op_list[MAX_TOTAL_OPS];
    int op_count = 0;
    for (int j = 0; j < njobs; ++j) {
        for (int o = 0; o < nops_per_job; ++o) {
            int op_node = op_to_node_idx(j, o, nops_per_job);
            if (op_node < 1 || op_node > num_ops_total) continue;
            op_list[op_count].job = j;
            op_list[op_count].op = o;
            op_list[op_count].est_time = est[op_node];
            op_list[op_count].machine = shop->plan[j][o].mach - 1;
            op_list[op_count].duration = shop->plan[j][o].len;
            op_count++;
        }
    }
    for (int i = 0; i < op_count - 1; i++) {
        for (int k = 0; k < op_count - i - 1; k++) {
            if (op_list[k].est_time > op_list[k + 1].est_time ||
                (op_list[k].est_time == op_list[k + 1].est_time && op_list[k].job > op_list[k + 1].job) ||
                (op_list[k].est_time == op_list[k + 1].est_time && op_list[k].job == op_list[k + 1].job && op_list[k].op > op_list[k + 1].op)) {
                OpScheduleInfo temp = op_list[k];
                op_list[k] = op_list[k + 1];
                op_list[k + 1] = temp;
            }
        }
    }
    for (int i = 0; i < op_count; i++) {
        int j = op_list[i].job;
        int o = op_list[i].op;
        int machine_idx = op_list[i].machine;
        int duration = op_list[i].duration;
        int earliest_start = op_list[i].est_time;
        if (o > 0) {
            int prev_end_time = shop->plan[j][o - 1].stime + shop->plan[j][o - 1].len;
            if (earliest_start < prev_end_time) {
                earliest_start = prev_end_time;
            }
        }
        if (earliest_start < machine_available_time[machine_idx]) {
            earliest_start = machine_available_time[machine_idx];
        }
        shop->plan[j][o].stime = earliest_start;
        machine_available_time[machine_idx] = earliest_start + duration;
    }
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
