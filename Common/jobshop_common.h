// jobshop_common.h
// Common definitions and structures for job-shop scheduling implementations
#ifndef JOBSHOP_COMMON_H
#define JOBSHOP_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

// Common constants
#define JMAX 100        // Maximum number of jobs
#define MMAX 100        // Maximum number of machines
#define OPMAX 100       // Maximum number of operations per job
#define LOGMAX 1000     // Maximum number of log entries (reduced to avoid stack overflow)
#define TMAX 32         // Maximum number of threads (for parallel version)

// Operation/Step structure
typedef struct {
    int mach;      // Machine ID for this operation
    int len;       // Duration/length of this operation
    int stime;     // Start time (-1 if not scheduled yet)
} Step;

// Definition for Shifting Bottleneck operations on a single machine
typedef struct {
    int job_idx;        // Original job index
    int op_idx_in_job;  // Original operation index within the job
    int op_node_id;     // Graph node ID for this operation
    int r_time;         // Release time (EST - Earliest Start Time)
    int p_time;         // Processing time
    int q_time_val;     // Tail time (longest path from this op to sink, including its p_time)
} OneMachineOpInfo;

// Log entry for timing analysis
typedef struct {
    int jid;          // Job ID
    int oid;          // Operation ID
    double tstart;    // Start time of scheduling operation
    double tspan;     // Time span for scheduling this operation
} LogEntry;

// Thread-specific log entry (for parallel version)
typedef struct {
    int jid;          // Job ID
    int oid;          // Operation ID
    double tstart;    // Start time of scheduling operation
    double tspan;     // Time span for scheduling this operation
} ThreadLog;

// Shop structure for sequential version
typedef struct {
    int njobs;                    // Number of jobs
    int nmachs;                   // Number of machines
    int nops;                     // Number of operations per job
    Step plan[JMAX][OPMAX];       // Static allocation: [job][operation]
    LogEntry logs[LOGMAX];        // Sequential logs
    int nlogs;                    // Number of log entries
} Shop;

// Common function declarations
void make_logs_dir(void);
int find_slot_seq(Shop *shop, int mach, int len, int earliest_start);

// Sequential version functions
int load_problem_seq(const char *filename, Shop *shop);
void save_result_seq(const char *filename, Shop *shop);
void reset_plan_seq(Shop *shop);
void dump_logs_seq(Shop *shop, const char *basename);

// Common utility functions
char* extract_basename(const char *filepath);

// New utility functions for automatic folder routing
const char* get_size_category(int njobs, int nmachs);
void create_algorithm_dirs(const char* algorithm);
void get_log_path(char* path, const char* algorithm, const char* size_category, const char* basename, const char* suffix);
void get_result_path(char* path, const char* algorithm, const char* size_category, const char* basename);

#endif // JOBSHOP_COMMON_H
