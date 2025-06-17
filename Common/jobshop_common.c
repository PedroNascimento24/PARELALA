// Implementation of common functions for job-shop scheduling

#include "jobshop_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h> // For mkdir

#ifdef _WIN32
#include <direct.h> // For _mkdir
#define MKDIR(path) _mkdir(path)
#else
#define MKDIR(path) mkdir(path, 0777) // 0777 are permissions
#endif

// Common function implementations

void make_logs_dir(void) {
    // Placeholder: Implement directory creation logic if it doesn't exist
    // For example: MKDIR("Logs");
    // This function might need to be more sophisticated if subdirectories per algorithm are needed here.
    // However, create_algorithm_dirs seems to handle specific algorithm log dirs.
    printf("make_logs_dir called (placeholder)\n");
}

int find_slot_seq(Shop *shop, int mach, int len, int earliest_start) {
    // Placeholder: Implement logic to find the earliest available time slot for an operation
    // on a given machine, considering already scheduled operations in shop->plan.
    // This is a simplified placeholder.
    int earliest_finish_time_on_machine = 0;
    for (int j = 0; j < shop->njobs; ++j) {
        for (int op = 0; op < shop->nops; ++op) {
            if (shop->plan[j][op].mach == mach && shop->plan[j][op].stime != -1) {
                int op_finish_time = shop->plan[j][op].stime + shop->plan[j][op].len;
                if (op_finish_time > earliest_finish_time_on_machine) {
                    earliest_finish_time_on_machine = op_finish_time;
                }
            }
        }
    }
    return (earliest_start > earliest_finish_time_on_machine) ? earliest_start : earliest_finish_time_on_machine;
}

int find_slot_par(ParallelShop *shop, int mach, int len, int earliest_start) {
    // Placeholder: Similar to find_slot_seq, but for ParallelShop structure.
    // Critical section might be needed if accessed concurrently by threads modifying the same machine's schedule.
    // This is a simplified placeholder.
    int earliest_finish_time_on_machine = 0;
    for (int j = 0; j < shop->njobs; ++j) {
        for (int op = 0; op < shop->nops; ++op) {
            if (shop->plan[j][op].mach == mach && shop->plan[j][op].stime != -1) {
                int op_finish_time = shop->plan[j][op].stime + shop->plan[j][op].len;
                if (op_finish_time > earliest_finish_time_on_machine) {
                    earliest_finish_time_on_machine = op_finish_time;
                }
            }
        }
    }
    return (earliest_start > earliest_finish_time_on_machine) ? earliest_start : earliest_finish_time_on_machine;
}

// Sequential version functions
int load_problem_seq(const char *filename, Shop *shop) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening problem file");
        return 0; // Failure
    }

    // First line: number of jobs, number of machines
    if (fscanf(file, "%d %d", &shop->njobs, &shop->nmachs) != 2) {
        fprintf(stderr, "Error reading njobs and nmachs from %s\n", filename);
        fclose(file);
        return 0;
    }
    // Assuming nops is uniform for all jobs and equal to nmachs as per common Taillard format.
    // If nops can vary or is specified differently, this needs adjustment.
    shop->nops = shop->nmachs; 


    if (shop->njobs > JMAX || shop->nmachs > MMAX || shop->nops > OPMAX) {
        fprintf(stderr, "Problem size exceeds maximum defined limits (JMAX, MMAX, OPMAX).\n");
        fclose(file);
        return 0;
    }
    
    // Subsequent lines: job operations (machine, duration)
    for (int i = 0; i < shop->njobs; ++i) {
        for (int k = 0; k < shop->nops; ++k) {
            if (fscanf(file, "%d %d", &shop->plan[i][k].mach, &shop->plan[i][k].len) != 2) {
                fprintf(stderr, "Error reading operation for job %d, op %d from %s\n", i, k, filename);
                fclose(file);
                return 0;
            }
            shop->plan[i][k].stime = -1; // Initialize start time as not scheduled
        }
    }

    fclose(file);
    shop->nlogs = 0;
    return 1; // Success
}

void save_result_seq(const char *filename, Shop *shop) {
    // This function signature matches the header.
    // The call in jobshop_seq_sb.c might need to be adjusted or this function adapted.
    // For now, implementing based on the header's signature.
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Error opening output file for saving results");
        return;
    }

    fprintf(file, "Number of jobs: %d\n", shop->njobs);
    fprintf(file, "Number of machines: %d\n", shop->nmachs);
    fprintf(file, "Number of operations per job: %d\n", shop->nops); // Assuming uniform

    int makespan = 0;
    for (int i = 0; i < shop->njobs; ++i) {
        for (int k = 0; k < shop->nops; ++k) {
            if (shop->plan[i][k].stime != -1) {
                int finish_time = shop->plan[i][k].stime + shop->plan[i][k].len;
                if (finish_time > makespan) {
                    makespan = finish_time;
                }
            }
        }
    }
    fprintf(file, "Makespan: %d\n\n", makespan);

    fprintf(file, "Job Operations (Job, Operation, Machine, Duration, Start Time):\n");
    for (int i = 0; i < shop->njobs; ++i) {
        for (int k = 0; k < shop->nops; ++k) {
            fprintf(file, "Job %d, Op %d: M%d, Len %d, Start %d\n",
                    i, k,
                    shop->plan[i][k].mach,
                    shop->plan[i][k].len,
                    shop->plan[i][k].stime);
        }
    }
    fclose(file);
    printf("Results saved to %s\n", filename);
}


void reset_plan_seq(Shop *shop) {
    for (int i = 0; i < shop->njobs; ++i) {
        for (int k = 0; k < shop->nops; ++k) {
            shop->plan[i][k].stime = -1; // Reset start times
        }
    }
    shop->nlogs = 0; // Reset log count
}

void dump_logs_seq(Shop *shop, const char *basename) {
    // Placeholder: Implement log dumping logic
    // Example: Create a log file and write shop->logs content
    char log_filename[1024];
    snprintf(log_filename, sizeof(log_filename), "Logs/%s_seq_log.txt", basename); // Simplified path
    
    FILE* log_file = fopen(log_filename, "w");
    if (!log_file) {
        perror("Failed to open sequential log file");
        return;
    }
    fprintf(log_file, "Sequential Log for %s\n", basename);
    fprintf(log_file, "JID,OID,TStart,TSpan\n");
    for(int i=0; i < shop->nlogs; ++i) {
        fprintf(log_file, "%d,%d,%.6f,%.6f\n", shop->logs[i].jid, shop->logs[i].oid, shop->logs[i].tstart, shop->logs[i].tspan);
    }
    fclose(log_file);
    printf("Sequential logs dumped to %s\n", log_filename);
}

// Parallel version functions
int load_problem_par(const char *filename, ParallelShop *shop) {
    // Placeholder: Similar to load_problem_seq but for ParallelShop
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening problem file for parallel load");
        return 0; // Failure
    }

    if (fscanf(file, "%d %d", &shop->njobs, &shop->nmachs) != 2) {
        fprintf(stderr, "Error reading njobs and nmachs from %s (parallel)\n", filename);
        fclose(file);
        return 0;
    }
    shop->nops = shop->nmachs; // Assuming uniform ops = nmachs

    if (shop->njobs > JMAX || shop->nmachs > MMAX || shop->nops > OPMAX) {
        fprintf(stderr, "Problem size exceeds maximum defined limits (JMAX, MMAX, OPMAX) for parallel load.\n");
        fclose(file);
        return 0;
    }

    for (int i = 0; i < shop->njobs; ++i) {
        for (int k = 0; k < shop->nops; ++k) {
            if (fscanf(file, "%d %d", &shop->plan[i][k].mach, &shop->plan[i][k].len) != 2) {
                fprintf(stderr, "Error reading operation for job %d, op %d from %s (parallel)\n", i, k, filename);
                fclose(file);
                return 0;
            }
            shop->plan[i][k].stime = -1;
        }
    }
    fclose(file);
    for(int t=0; t<TMAX; ++t) shop->tlogc[t] = 0;
    return 1; // Success
}

void save_result_par(const char *filename, ParallelShop *shop) {
    // Placeholder: Similar to save_result_seq but for ParallelShop
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Error opening output file for saving parallel results");
        return;
    }

    fprintf(file, "Number of jobs: %d\n", shop->njobs);
    fprintf(file, "Number of machines: %d\n", shop->nmachs);
    fprintf(file, "Number of operations per job: %d\n", shop->nops);

    int makespan = 0;
    for (int i = 0; i < shop->njobs; ++i) {
        for (int k = 0; k < shop->nops; ++k) {
            if (shop->plan[i][k].stime != -1) {
                int finish_time = shop->plan[i][k].stime + shop->plan[i][k].len;
                if (finish_time > makespan) {
                    makespan = finish_time;
                }
            }
        }
    }
    fprintf(file, "Makespan: %d\n\n", makespan);

    fprintf(file, "Job Operations (Job, Operation, Machine, Duration, Start Time):\n");
    for (int i = 0; i < shop->njobs; ++i) {
        for (int k = 0; k < shop->nops; ++k) {
            fprintf(file, "Job %d, Op %d: M%d, Len %d, Start %d\n",
                    i, k,
                    shop->plan[i][k].mach,
                    shop->plan[i][k].len,
                    shop->plan[i][k].stime);
        }
    }
    fclose(file);
    printf("Parallel results saved to %s\n", filename);
}

void reset_plan_par(ParallelShop *shop) {
    for (int i = 0; i < shop->njobs; ++i) {
        for (int k = 0; k < shop->nops; ++k) {
            shop->plan[i][k].stime = -1;
        }
    }
    for (int t = 0; t < TMAX; ++t) {
        shop->tlogc[t] = 0;
    }
}

void dump_logs_par(ParallelShop *shop, int num_threads, const char *basename) {
    // Placeholder: Implement parallel log dumping
    char log_filename[1024];
    snprintf(log_filename, sizeof(log_filename), "Logs/%s_par_log.txt", basename); // Simplified path

    FILE* log_file = fopen(log_filename, "w");
    if (!log_file) {
        perror("Failed to open parallel log file");
        return;
    }
    fprintf(log_file, "Parallel Log for %s with %d threads\n", basename, num_threads);
    for(int t=0; t < num_threads; ++t) {
        fprintf(log_file, "--- Thread %d Logs (%d entries) ---\n", t, shop->tlogc[t]);
        fprintf(log_file, "JID,OID,TStart,TSpan\n");
        for(int i=0; i < shop->tlogc[t]; ++i) {
            fprintf(log_file, "%d,%d,%.6f,%.6f\n", shop->tlogs[t][i].jid, shop->tlogs[t][i].oid, shop->tlogs[t][i].tstart, shop->tlogs[t][i].tspan);
        }
    }
    fclose(log_file);
    printf("Parallel logs dumped to %s\n", log_filename);
}

// Common utility functions
char* extract_basename(const char *filepath) {
    const char *last_slash = strrchr(filepath, '/');
    const char *last_backslash = strrchr(filepath, '\\');
    const char *filename_start = filepath;

    if (last_slash) {
        filename_start = last_slash + 1;
    }
    if (last_backslash && last_backslash + 1 > filename_start) {
        filename_start = last_backslash + 1;
    }

    char *basename_alloc = strdup(filename_start);
    if (!basename_alloc) {
        fprintf(stderr, "strdup failed in extract_basename\n");
        return NULL;
    }

    // Remove extension if present
    char *last_dot = strrchr(basename_alloc, '.');
    if (last_dot && last_dot != basename_alloc) { // Ensure dot is not the first character
        *last_dot = '\0';
    }
    return basename_alloc; // Remember to free this
}

// New utility functions for automatic folder routing
const char* get_size_category(int njobs, int nmachs) {
    // Placeholder: Implement logic based on njobs/nmachs to categorize
    // This is a very basic example.
    if (njobs <= 10 && nmachs <= 10) return "P1_Small";
    if (njobs <= 20 && nmachs <= 20) return "P2_Medium";
    if (njobs <= 50 && nmachs <= 50) return "P3_Large";
    if (njobs <= 100 && nmachs <= 100) return "P4_XLarge";
    return "P5_XXLarge";
}

void create_directory_if_not_exists(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (MKDIR(path) != 0 && errno != EEXIST) { // Check errno for EEXIST in case of race
            fprintf(stderr, "Error creating directory %s: %s\n", path, strerror(errno));
        }
    }
}

void create_algorithm_dirs(const char* algorithm) {
    // Creates base Log and Result directories if they don't exist
    // Then creates subdirectories for the given algorithm and size categories.
    char base_log_path[512];
    char base_result_path[512];

    // Assuming script runs from PWD = project root/Scripts, so ../Logs and ../Results
    // If running from project root, then "Logs" and "Results"
    // For robustness, let's assume these are relative to where the executable might be or a known root.
    // The build script places executables in Algorithms/<AlgName>/
    // So, ../../Logs and ../../Results from executable's perspective.
    // This needs to be robust. For now, using relative paths that might work if CWD is consistent.
    // A better approach is to pass base paths or use absolute paths.

    snprintf(base_log_path, sizeof(base_log_path), "../../Logs");
    snprintf(base_result_path, sizeof(base_result_path), "../../Results");
    
    create_directory_if_not_exists(base_log_path);
    create_directory_if_not_exists(base_result_path);

    char path_buffer[1024];

    // Log directories
    snprintf(path_buffer, sizeof(path_buffer), "%s/%s", base_log_path, algorithm);
    create_directory_if_not_exists(path_buffer);
    const char* categories[] = {"P1_Small", "P2_Medium", "P3_Large", "P4_XLarge", "P5_XXLarge"};
    for (int i = 0; i < sizeof(categories)/sizeof(categories[0]); ++i) {
        snprintf(path_buffer, sizeof(path_buffer), "%s/%s/%s", base_log_path, algorithm, categories[i]);
        create_directory_if_not_exists(path_buffer);
    }

    // Result directories
    snprintf(path_buffer, sizeof(path_buffer), "%s/%s", base_result_path, algorithm);
    create_directory_if_not_exists(path_buffer);
    for (int i = 0; i < sizeof(categories)/sizeof(categories[0]); ++i) {
        snprintf(path_buffer, sizeof(path_buffer), "%s/%s/%s", base_result_path, algorithm, categories[i]);
        create_directory_if_not_exists(path_buffer);
    }
}


void get_log_path(char* path_buffer, const char* algorithm, const char* size_category, const char* basename, const char* suffix) {
    // Assuming executables are in Algorithms/<AlgorithmName>/
    // So logs are in ../../Logs/
    snprintf(path_buffer, 1024, "../../Logs/%s/%s/%s_%s.txt",
             algorithm, size_category, basename, suffix);
}

void get_result_path(char* path_buffer, const char* algorithm, const char* size_category, const char* basename) {
    // Assuming executables are in Algorithms/<AlgorithmName>/
    // So results are in ../../Results/
    snprintf(path_buffer, 1024, "../../Results/%s/%s/%s_results.txt", // Standardized result filename
             algorithm, size_category, basename);
}
