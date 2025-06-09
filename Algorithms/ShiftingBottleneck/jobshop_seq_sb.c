// jobshop_seq_sb.c
// Sequential job shop scheduler using Shifting Bottleneck heuristic

#include "../../Common/jobshop_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h> // For mkdir on POSIX

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
static AdjList adj[MAX_GRAPH_NODES];
static AdjList rev_adj[MAX_GRAPH_NODES]; // For calculating tails
static int node_proc_times[MAX_GRAPH_NODES]; // Processing time of each node (operation)
static int est[MAX_GRAPH_NODES];      // Earliest Start Times
static int tail_q[MAX_GRAPH_NODES];   // Tails (longest path from op to sink)


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
// Node 0 is Source, Node 1 to num_ops_total are operations, Node num_ops_total+1 is Sink
int op_to_node_idx(int job_idx, int op_idx_in_job, int ops_per_job_param) {
    // Assuming ops_per_job_param > 0, which is checked in shifting_bottleneck_schedule
    return 1 + job_idx * ops_per_job_param + op_idx_in_job;
}


// Calculate Earliest Start Times (EST) for all nodes in a DAG using topological sort
// est[i] = earliest start time of operation i.
// node_proc_times[i] = processing time of operation i.
// For an edge u->v, est[v] = max(est[v], est[u] + node_proc_time[u]).
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
    int head = 0, tail = 0;

    // Enqueue all nodes with in-degree 0
    // Specifically, the source_node_idx should be among these if it's a valid source.
    for (int i = 0; i < num_total_nodes; ++i) {
        if (in_degree[i] == 0) {
            queue[tail++] = i;
        }
    }
    
    result_est[source_node_idx] = 0; // EST of source is 0

    int count_processed_nodes = 0;
    while (head < tail) {
        int u = queue[head++];
        count_processed_nodes++;

        AdjListNode* p_crawl = current_adj[u].head;
        while (p_crawl) {
            int v = p_crawl->dest;
            // For EST: est[v] = max(est[v], est[u] + proc_time_of_u)
            // For Tails (using reversed graph): tail[v] = max(tail[v], tail[u] + proc_time_of_v)
            // The proc_time used depends on whether it's EST or Tail calc.
            // For EST, it's proc_time of the *source* of the edge (u).
            // For Tails (where u is a successor and v is a predecessor in original graph),
            // it's proc_time of the *destination* of the edge in reversed graph (which is u, original predecessor).
            // The current_node_proc_times array should be indexed by the node whose processing time is being added.
            if (result_est[v] < result_est[u] + current_node_proc_times[u]) {
                result_est[v] = result_est[u] + current_node_proc_times[u];
            }
            in_degree[v]--;
            if (in_degree[v] == 0) {
                queue[tail++] = v;
            }
            p_crawl = p_crawl->next;
        }
    }

    if (count_processed_nodes < num_total_nodes) {
        // This indicates a cycle if not all nodes are processed.
        // Or some nodes are unreachable from the initial set of in-degree 0 nodes.
        // For SB, the graph should remain a DAG.
        // Check if all nodes were expected to be reachable from source_node_idx.
        // fprintf(stderr, "Warning: Cycle detected or disconnected graph in EST calculation (%d/%d nodes processed).\n", count_processed_nodes, num_total_nodes);
    }

    free(in_degree);
    free(queue);
}


// Main Shifting Bottleneck scheduling logic
void shifting_bottleneck_schedule(Shop *shop) {
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

    // Build initial graph with conjunctive arcs and set processing times for operation nodes
    for (int j = 0; j < njobs; ++j) {
        for (int o = 0; o < nops_per_job; ++o) {
            int current_op_node = op_to_node_idx(j, o, nops_per_job);
            if(current_op_node < 1 || current_op_node > num_ops_total) { /*fprintf(stderr, "Invalid node index %d for job %d op %d\n", current_op_node, j,o);*/ continue; }

            node_proc_times[current_op_node] = shop->plan[j][o].len;

            if (o == 0) { // Connect Source to the first operation of each job
                add_graph_edge(adj, source_node, current_op_node);
            }
            if (o < nops_per_job - 1) { // Connect current op to the next op in the same job
                int next_op_node = op_to_node_idx(j, o + 1, nops_per_job);
                if(next_op_node < 1 || next_op_node > num_ops_total) { /*fprintf(stderr, "Invalid next_op_node index\n");*/ continue; }
                add_graph_edge(adj, current_op_node, next_op_node);
            }
            if (o == nops_per_job - 1) { // Connect the last operation of each job to Sink
                add_graph_edge(adj, current_op_node, sink_node);
            }
        }
    }

    int sequenced_machines_flags[MMAX] = {0}; // 1 if machine m is sequenced, 0 otherwise
    int num_sequenced_machines_count = 0;

    OneMachineOpInfo* machine_ops_buffer = (OneMachineOpInfo*)malloc(JMAX * sizeof(OneMachineOpInfo)); // Max JMAX ops on one machine
    int* current_sequence_nodes_buffer = (int*)malloc(JMAX * sizeof(int));
    int* best_sequence_for_bottleneck_machine = (int*)malloc(JMAX * sizeof(int));

    if (!machine_ops_buffer || !current_sequence_nodes_buffer || !best_sequence_for_bottleneck_machine) {
        fprintf(stderr, "Failed to allocate memory for SB buffers\n");
        if(machine_ops_buffer) free(machine_ops_buffer);
        if(current_sequence_nodes_buffer) free(current_sequence_nodes_buffer);
        if(best_sequence_for_bottleneck_machine) free(best_sequence_for_bottleneck_machine);
        free_graph_adj_list(adj, num_graph_nodes);
        // rev_adj might not be built yet, but free_graph_adj_list handles NULL heads.
        free_graph_adj_list(rev_adj, num_graph_nodes); 
        return;
    }

    while (num_sequenced_machines_count < shop->nmachs) {
        int best_machine_to_sequence_idx = -1; 
        long long max_bottleneck_metric_val = -1; 
        int best_found_sequence_len = 0;

        // Calculate current ESTs (r_j values) based on graph from Source
        calculate_est_AON(source_node, num_graph_nodes, adj, node_proc_times, est);
        
        // Calculate current tails (q_j values)
        // 1. Build reversed graph (adj -> rev_adj)
        free_graph_adj_list(rev_adj, num_graph_nodes); // Clear previous rev_adj
        for(int u_node=0; u_node<num_graph_nodes; ++u_node) {
            AdjListNode* p_crawl = adj[u_node].head;
            while(p_crawl){
                add_graph_edge(rev_adj, p_crawl->dest, u_node); // Add reversed edge
                p_crawl = p_crawl->next;
            }
        }
        // 2. Calculate "EST" on reversed graph, starting from original Sink node.
        // For tails, the "processing time" added at each step is that of the *destination* node of the edge in the reversed graph
        // which corresponds to the *source* node of the edge in the original graph.
        // To use calculate_est_AON directly for tails, we need a proc_times array suitable for it.
        // Let's define tail_q[i] = longest path from i to Sink, including proc_time[i].
        // So, when traversing u->v in reversed graph (v->u in original), tail_q[v] = max(tail_q[v], tail_q[u] + proc_time[v])
        // This means the proc_times array passed to calculate_est_AON should be the original node_proc_times.
        // The result_est from this call will be our tail_q values.
        // The source for this calculation is the SINK node of the original graph.
        calculate_est_AON(sink_node, num_graph_nodes, rev_adj, node_proc_times, tail_q);
        // After this, tail_q[i] is the longest path from SINK to i in reversed graph.
        // This is equivalent to longest path from i to SINK in original graph.
        // The definition of q_j in SB often means "sum of processing times on the critical path from the START of operation j to the end of the project, EXCLUDING p_j".
        // Or "sum of processing times on critical path from END of op j to end of project".
        // If calculate_est_AON gives longest path from SINK to i using proc_times[k] for edge j->k (reversed),
        // then tail_q[i] = longest path from i to SINK.
        // Let's adjust: tail_q[i] should be longest path from START of op i to SINK, *including* p_i.
        // The calculate_est_AON computes L(j) = max_{k in P(j)} (L(k) + t_k). If t_k is proc time of k.
        // For tails, if we reverse graph, and compute L'(j) from sink S, L'(j) = max_{k in S'(j)} (L'(k) + t_k_prime)
        // If t_k_prime is proc_time of node k (original node), then L'(j) is longest path from S to j.
        // This is what we need: tail_q[op_node] = result_est[op_node] from this call.

        for (int m_idx = 0; m_idx < shop->nmachs; ++m_idx) { // m_idx is 0 to nmachs-1
            if (sequenced_machines_flags[m_idx]) continue;

            int num_ops_on_this_machine = 0;
            int stop_collecting_for_this_machine = 0;
            for (int j = 0; j < njobs; ++j) {
                for (int o = 0; o < nops_per_job; ++o) {
                    if (shop->plan[j][o].mach == m_idx + 1) { // Machines in problem are 1-indexed
                        if (num_ops_on_this_machine >= JMAX) {
                            stop_collecting_for_this_machine = 1;
                            break; 
                        }
                        int op_node = op_to_node_idx(j, o, nops_per_job);
                        if(op_node < 1 || op_node > num_ops_total) continue;

                        machine_ops_buffer[num_ops_on_this_machine].job_idx = j;
                        machine_ops_buffer[num_ops_on_this_machine].op_idx_in_job = o;
                        machine_ops_buffer[num_ops_on_this_machine].op_node_id = op_node;
                        machine_ops_buffer[num_ops_on_this_machine].p_time = node_proc_times[op_node];
                        machine_ops_buffer[num_ops_on_this_machine].r_time = est[op_node]; // EST of the operation node itself
                        // tail_q[op_node] is longest path from op_node to Sink, including proc_time[op_node].
                        // For Lmax type calculations, q_j is often time *after* op_node. So, tail_q[op_node] - node_proc_times[op_node].
                        // Let's use the full tail_q[op_node] for now as a general measure of "criticality".
                        machine_ops_buffer[num_ops_on_this_machine].q_time_val = tail_q[op_node]; 
                        num_ops_on_this_machine++;
                    }
                }
                if (stop_collecting_for_this_machine) {
                    break; 
                }
            }

            if (num_ops_on_this_machine == 0) continue;

            // Simple heuristic for 1-machine: Sort by r_j (EST), then p_j (proc_time) as tie-breaker
            // This is a variation of Earliest Release Time heuristic.
            for(int i=0; i<num_ops_on_this_machine-1; ++i) {
                for(int k=0; k<num_ops_on_this_machine-i-1; ++k) {
                    if(machine_ops_buffer[k].r_time > machine_ops_buffer[k+1].r_time ||
                       (machine_ops_buffer[k].r_time == machine_ops_buffer[k+1].r_time && machine_ops_buffer[k].p_time > machine_ops_buffer[k+1].p_time)) {
                        OneMachineOpInfo temp_op_info = machine_ops_buffer[k];
                        machine_ops_buffer[k] = machine_ops_buffer[k+1];
                        machine_ops_buffer[k+1] = temp_op_info;
                    }
                }
            }
            
            long long current_machine_completion_time = 0;
            long long current_machine_cmax_val = 0; // Cmax for this specific machine schedule
            for (int i = 0; i < num_ops_on_this_machine; ++i) {
                // Start time of op i on this machine is max(its EST, completion time of previous op on this machine)
                current_machine_completion_time = (current_machine_completion_time > machine_ops_buffer[i].r_time) ? current_machine_completion_time : machine_ops_buffer[i].r_time;
                current_machine_completion_time += machine_ops_buffer[i].p_time; // Add its processing time
                
                if (current_machine_completion_time > current_machine_cmax_val) { // This is C_i
                    current_machine_cmax_val = current_machine_completion_time;
                }
                current_sequence_nodes_buffer[i] = machine_ops_buffer[i].op_node_id;
            }
            
            // Bottleneck measure: Cmax on this machine using the heuristic sequence.
            // A more sophisticated measure might be Lmax if due dates were involved, or critical path length through this machine.
            long long current_bottleneck_metric = current_machine_cmax_val; 

            if (current_bottleneck_metric > max_bottleneck_metric_val) {
                max_bottleneck_metric_val = current_bottleneck_metric;
                best_machine_to_sequence_idx = m_idx;
                best_found_sequence_len = num_ops_on_this_machine;
                for(int i=0; i<num_ops_on_this_machine; ++i) {
                    best_sequence_for_bottleneck_machine[i] = current_sequence_nodes_buffer[i];
                }
            }
        }

        if (best_machine_to_sequence_idx == -1) {
            // This might happen if all remaining machines have no pending operations, 
            // or if all machines are sequenced.
            if (num_sequenced_machines_count < shop->nmachs) {
                 // This could be an issue if there are still machines to sequence but none yield a positive bottleneck metric.
                 // For simplicity, we break. A robust SB might need to handle this (e.g. pick one randomly or by another criterion).
                 // fprintf(stderr, "SB Warning: Could not find a bottleneck machine, but not all machines sequenced (%d/%d).\n", num_sequenced_machines_count, shop->nmachs);
            }
            break; 
        }

        // Add disjunctive arcs for the chosen bottleneck machine based on its determined sequence
        for (int i = 0; i < best_found_sequence_len - 1; ++i) {
            int u_node = best_sequence_for_bottleneck_machine[i];
            int v_node = best_sequence_for_bottleneck_machine[i+1];
            if (u_node < 1 || u_node > num_ops_total || v_node < 1 || v_node > num_ops_total) { /*fprintf(stderr, "Invalid node in best_sequence\n");*/ continue;}
            add_graph_edge(adj, u_node, v_node); // Add forward edge in the main graph
        }
        sequenced_machines_flags[best_machine_to_sequence_idx] = 1;
        num_sequenced_machines_count++;
    }

    // All machines are sequenced (or no more bottlenecks found).
    // Calculate final ESTs for all operations based on the fully augmented graph.
    calculate_est_AON(source_node, num_graph_nodes, adj, node_proc_times, est);

    // Populate shop->plan[j][o].stime with these final ESTs
    for (int j = 0; j < njobs; ++j) {
        for (int o = 0; o < nops_per_job; ++o) {
            int op_node = op_to_node_idx(j, o, nops_per_job);
            if(op_node < 1 || op_node > num_ops_total) continue;
            shop->plan[j][o].stime = est[op_node];
        }
    }
    
    // Free dynamically allocated memory for graph adjacency lists and buffers
    free_graph_adj_list(adj, num_graph_nodes);
    free_graph_adj_list(rev_adj, num_graph_nodes);
    free(machine_ops_buffer);
    free(current_sequence_nodes_buffer);
    free(best_sequence_for_bottleneck_machine);
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <problem_file> <output_file_suffix>\n", argv[0]);
        fprintf(stderr, "Example: .\jobshop_seq_sb.exe ..\..\Instances\P1_Small\taillard_3_3_1.txt seq_sb\n");
        return 1;
    }
    char *problem_file = argv[1];
    char *output_suffix = argv[2]; 

    Shop shop_instance; 
    memset(&shop_instance, 0, sizeof(Shop));

    if (!load_problem_seq(problem_file, &shop_instance)) {
        fprintf(stderr, "Error loading problem from %s\n", problem_file);
        return 1;
    }

    char *basename = extract_basename(problem_file);
    if (!basename) {
        fprintf(stderr, "Error extracting basename from %s. Using 'unknown'.\n", problem_file);
        basename = strdup("unknown"); // Remember to free this
        if (!basename) {
            fprintf(stderr, "strdup failed for basename fallback.\n");
            return 1;
        }
    }
    
    const char* algorithm_name = "ShiftingBottleneck";
    
    printf("Starting Sequential Shifting Bottleneck for %s...\n", basename);
    shifting_bottleneck_schedule(&shop_instance);
    printf("Sequential Shifting Bottleneck finished for %s.\n", basename);

    const char* size_cat = get_size_category(shop_instance.njobs, shop_instance.nmachs);
    char result_path[1024];
    char path_buf[512]; // For individual directory components

#ifdef _WIN32
    _mkdir("../../Result");
    sprintf(path_buf, "../../Result/%s", algorithm_name); _mkdir(path_buf);
    sprintf(path_buf, "../../Result/%s/%s", algorithm_name, size_cat); _mkdir(path_buf);
#else
    mkdir("../../Result", 0777); // S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH
    sprintf(path_buf, "../../Result/%s", algorithm_name); mkdir(path_buf, 0777);
    sprintf(path_buf, "../../Result/%s/%s", algorithm_name, size_cat); mkdir(path_buf, 0777);
#endif

    sprintf(result_path, "../../Result/%s/%s/%s_%s.txt", algorithm_name, size_cat, basename, output_suffix);
    
    save_result_seq(result_path, &shop_instance); 
    printf("Results saved to %s\n", result_path);
    
    if (basename) free(basename); 
    return 0;
}
