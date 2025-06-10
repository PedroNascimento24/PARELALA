// jobshop_seq_spt.c
// Sequential job shop scheduler using Shortest Processing Time (SPT) heuristic
#include "../../Common/jobshop_common.h"
#include <stdio.h> // Included for FILE, printf, sprintf, fopen, fclose, perror, snprintf
#include <stdlib.h> // Included for system, atoi, malloc, free, exit, getenv
#include <string.h> // Included for strcpy, strrchr, strcmp, strlen, strncpy
#include <time.h>   // Included for clock, CLOCKS_PER_SEC
#include <sys/stat.h> // For stat, mkdir

#ifdef _WIN32
#include <windows.h> // For LARGE_INTEGER, QueryPerformanceFrequency, QueryPerformanceCounter
#include <direct.h>  // For _mkdir
#define MKDIR(path) _mkdir(path)
#else
#include <unistd.h> // For usleep
#define MKDIR(path) mkdir(path, 0777)
#endif

// SPT Scheduling: Always select operation with shortest processing time
void spt_schedule(Shop *shop_instance) { // Renamed shop to shop_instance
    int done[JMAX] = {0};
    int nextst[JMAX] = {0};
    int total = shop_instance->njobs * shop_instance->nops, count = 0;
    while (count < total) {
        int bestj = -1, besto = -1, bestlen = 999999;
        
        // Find the operation with shortest processing time among available operations
        for (int j = 0; j < shop_instance->njobs; j++) {
            if (done[j] < shop_instance->nops) {
                int o = done[j];
                int len = shop_instance->plan[j][o].len;
                if (len < bestlen) {
                    bestlen = len;
                    bestj = j;
                    besto = o;
                }
            }
        }
        
        if (bestj == -1) break; // Should not happen in a valid problem if count < total
        
        int m = shop_instance->plan[bestj][besto].mach;
        int l = shop_instance->plan[bestj][besto].len;
        clock_t t0 = clock();
        // Use common find_slot_seq
        int st = find_slot_seq(shop_instance, m, l, nextst[bestj]);
        shop_instance->plan[bestj][besto].stime = st;
        done[bestj]++;
        count++;
        if (done[bestj] < shop_instance->nops) nextst[bestj] = st + l;
        clock_t t1 = clock();
        double dt = ((double)(t1-t0))/CLOCKS_PER_SEC;
        if (shop_instance->nlogs < LOGMAX) {
            shop_instance->logs[shop_instance->nlogs].jid = bestj;
            shop_instance->logs[shop_instance->nlogs].oid = besto;
            shop_instance->logs[shop_instance->nlogs].tstart = ((double)t0)/CLOCKS_PER_SEC;
            shop_instance->logs[shop_instance->nlogs].tspan = dt;
            shop_instance->nlogs++;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3 && argc != 2) { // Allow 2 arguments if output is fully auto-routed
        fprintf(stderr, "Usage: %s <input_file> [output_file_deprecated]\\n", argv[0]);
        return 1;
    }
    char problem_path_arg[256]; // Input file path from argument
    // char output_path_arg[256]; // User-specified output file (optional, can be deprecated)

    strcpy(problem_path_arg, argv[1]);
    // if (argc == 3) {
    //     strcpy(output_path_arg, argv[2]);
    // } else {
    //     output_path_arg[0] = '\\0'; // No user-specified output
    // }
    
    char* base_filename = extract_basename(problem_path_arg);
    if (!base_filename) {
        fprintf(stderr, "Error extracting basename from %s\\n", problem_path_arg);
        return 1;
    }

    Shop shop_instance; // Renamed shop to shop_instance
    // Use common load_problem_seq
    if (!load_problem_seq(problem_path_arg, &shop_instance)) {
        fprintf(stderr, "Failed to load problem: %s\\n", problem_path_arg);
        free(base_filename);
        return 1;
    }
    
    const char* algorithm_name = "SPT"; // Algorithm name for path generation
    // Use common get_size_category
    const char* size_cat = get_size_category(shop_instance.njobs, shop_instance.nmachs);
    
    // Create necessary directories for logs and results using common function
    create_algorithm_dirs(algorithm_name); // Ensures ../../Logs/SPT/P1_Small etc. exist

    char auto_result_path[1024];
    // Use common get_result_path (4 arguments)
    get_result_path(auto_result_path, algorithm_name, size_cat, base_filename);
    
    // Performance measurement
#ifdef _WIN32
    LARGE_INTEGER freq, t_start, t_end;
    QueryPerformanceFrequency(&freq);
#else
    struct timespec t_start, t_end;
#endif
    double total_duration_seconds = 0.0;
    const int reps = 1; // Set to 1 for typical runs, can be increased for micro-benchmarking

    for (int i = 0; i < reps; i++) {
        reset_plan_seq(&shop_instance); // Use common reset_plan_seq
        // if (i < reps - 1) shop_instance.nlogs = 0; // nlogs is reset in reset_plan_seq

#ifdef _WIN32
        QueryPerformanceCounter(&t_start);
#else
        clock_gettime(CLOCK_MONOTONIC, &t_start);
#endif
        spt_schedule(&shop_instance); // Call the algorithm-specific scheduler
#ifdef _WIN32
        QueryPerformanceCounter(&t_end);
        total_duration_seconds += (double)(t_end.QuadPart - t_start.QuadPart) / freq.QuadPart;
#else
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        total_duration_seconds += (t_end.tv_sec - t_start.tv_sec) + (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
#endif
    }
    double avg_duration_seconds = total_duration_seconds / reps;
    
    // Use common dump_logs_seq (2 arguments: shop_instance, qualified_basename for log)
    // Construct qualified_basename for logs if needed, or just base_filename
    // The common dump_logs_seq expects a base name for the log file.
    // It internally constructs Logs/basename_seq_log.txt. This might need adjustment
    // if we want Logs/Algorithm/SizeCategory/basename_seq_log.txt
    // For now, let's assume dump_logs_seq is adapted or we pass a more specific basename.
    // The common dump_logs_seq has been updated to use a simpler path, let's use base_filename.
    // However, the previous local dump_logs_seq created specific timing and sequence files.
    // The common dump_logs_seq creates one log file. This is a change in behavior.
    // Let's call the common one for now.
    // The common dump_logs_seq takes (Shop*, const char* basename)
    // The basename should be something like "SPT/P1_Small/problem_basename_seqspt" for the old log structure
    // Or, if dump_logs_seq is to use get_log_path, it needs more info.
    // The current common dump_logs_seq is: snprintf(log_filename, sizeof(log_filename), "Logs/%s_seq_log.txt", basename);
    // This is not routing to Algorithm/SizeCategory.
    // This needs to be harmonized. For now, I will call it with base_filename.
    // The PDF asks for specific log paths.
    // Let's prepare the specific log path and pass it to a modified dump_logs_seq or handle logging here.

    // For now, stick to the common dump_logs_seq signature and its current behavior.
    // The common `dump_logs_seq` takes `Shop*` and `basename`.
    // It creates `Logs/basename_seq_log.txt`. This is not what we want.
    // The `jobshop_common.h` declares `void dump_logs_seq(Shop *shop, const char *basename);`
    // Let's assume the common `dump_logs_seq` is updated to take the full path or a template.
    // The implementation in common.c is: `snprintf(log_filename, sizeof(log_filename), "Logs/%s_seq_log.txt", basename);`
    // This is inconsistent. The header is `const char *basename` but the implementation uses it as a simple prefix.

    // Re-evaluating: The `dump_logs_seq` in `jobshop_common.h` is `void dump_logs_seq(Shop *shop, const char *basename)`
    // The implementation in `jobshop_common.c` is `void dump_logs_seq(Shop *shop, const char *basename)`
    // and it creates `Logs/basename_seq_log.txt`.
    // This means the `basename` argument should be `Algorithm/SizeCategory/problem_base` for it to work with the desired structure.
    // Or, `dump_logs_seq` itself should call `get_log_path`.
    // The `Greedy` refactor used `dump_logs_seq(&shop_instance, qualified_basename_for_log);`
    // where `qualified_basename_for_log` was `sprintf(qualified_basename_for_log, "%s/%s/%s_greedy_seq", algorithm_name, size_cat, base_filename);`
    // This means `dump_logs_seq` would create `Logs/SPT/P1_Small/problem_greedy_seq_seq_log.txt` which is not quite right.

    // Let's assume `dump_logs_seq` in `jobshop_common.c` is intended to be simple, and the main controls the full path.
    // The header for `dump_logs_seq` is `(Shop *shop, const char *basename)`.
    // The `Greedy` refactor called `dump_logs_seq(&shop_instance, qualified_basename_for_log);`
    // Let's assume `dump_logs_seq` is meant to take a *base name* and it constructs the *final log file name itself*.
    // The `Greedy` refactor passed a `qualified_basename` which included `algorithm/size_category`.
    // So, if `basename` is `SPT/P1_Small/base_filename`, the log would be `Logs/SPT/P1_Small/base_filename_seq_log.txt`. This seems correct.

    char qualified_log_basename[512];
    snprintf(qualified_log_basename, sizeof(qualified_log_basename), "%s/%s/%s", algorithm_name, size_cat, base_filename);
    dump_logs_seq(&shop_instance, qualified_log_basename); // Call common dump_logs_seq

    // Save results using common save_result_seq
    save_result_seq(auto_result_path, &shop_instance);
    // if (argc == 3 && strlen(output_path_arg) > 0) {
    //     save_result_seq(output_path_arg, &shop_instance); // Save to user-specified path if provided
    // }
    printf("Sequential SPT results saved to %s\\n", auto_result_path);
    
    // Append execution time to a summary log file (specific to this algorithm's main)
    char summary_log_path[1024];
    // Use common get_log_path for the summary timing log. Suffix can be "exec_summary"
    get_log_path(summary_log_path, algorithm_name, size_cat, base_filename, "exec_times_seq");
    
    FILE *fsum = fopen(summary_log_path, "a"); // Append mode
    if (fsum) {
        fprintf(fsum, "Input: %s, Algorithm: %s_SEQ, AvgTime: %.9f s, Makespan: %d\\n", 
                base_filename, algorithm_name, avg_duration_seconds, get_makespan_seq(&shop_instance));
        fclose(fsum);
    } else {
        perror("Failed to open summary log file for appending");
    }

    free(base_filename);
    return 0;
}

// Helper to get makespan, can be added to common if used by others, or kept local if specific format needed by main
int get_makespan_seq(Shop *shop_instance) {
    int max_end_time = 0;
    for (int j = 0; j < shop_instance->njobs; j++) {
        for (int o = 0; o < shop_instance->nops; o++) {
            if (shop_instance->plan[j][o].stime != -1) {
                int end_time = shop_instance->plan[j][o].stime + shop_instance->plan[j][o].len;
                if (end_time > max_end_time) {
                    max_end_time = end_time;
                }
            }
        }
    }
    return max_end_time;
}
