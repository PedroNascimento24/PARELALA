// jobshop_par_greedy.c
// Parallel job shop scheduler using parallel greedy heuristic
#include "../../Common/jobshop_common.h"
#include <omp.h>

// int load_problem_par(const char *fname, ParallelShop *shop) {
//     FILE *f = fopen(fname, "r");
//     if (!f) return 0;
//     int r = fscanf(f, "%d %d", &shop->njobs, &shop->nmachs);
//     if (r != 2) { fclose(f); return 0; }
//     shop->nops = shop->nmachs;
    
//     // Read problem data into static arrays
//     for (int j = 0; j < shop->njobs; j++) {
//         for (int o = 0; o < shop->nops; o++) {
//             int rr = fscanf(f, "%d %d", &shop->plan[j][o].mach, &shop->plan[j][o].len);
//             if (rr != 2) { fclose(f); return 0; }
//             shop->plan[j][o].stime = -1;
//         }
//     }
    
//     // Initialize thread log counters
//     for (int t = 0; t < TMAX; t++) shop->tlogc[t] = 0;
//     fclose(f);
//     return 1;
// }

// void save_result_par(const char *fname, ParallelShop *shop) {
//     // Create directory structure first
//     char dir_path[512];
//     char *last_slash = strrchr(fname, '/');
//     char *last_bslash = strrchr(fname, '\\');
//     char *last_sep = (last_slash > last_bslash) ? last_slash : last_bslash;
    
//     if (last_sep) {
//         strncpy(dir_path, fname, last_sep - fname);
//         dir_path[last_sep - fname] = '\0';
        
// #ifdef _WIN32
//         char cmd[1024];
//         sprintf(cmd, "mkdir \"%s\" 2>nul", dir_path);
//         system(cmd);
// #else
//         char cmd[1024];
//         sprintf(cmd, "mkdir -p \"%s\"", dir_path);
//         system(cmd);
// #endif
//     }
    
//     FILE *f = fopen(fname, "w");
//     if (!f) return;
//     int maxend = 0;
//     for (int j = 0; j < shop->njobs; j++)
//         for (int o = 0; o < shop->nops; o++) {
//             int e = shop->plan[j][o].stime + shop->plan[j][o].len;
//             if (e > maxend) maxend = e;
//         }
//     fprintf(f, "%d\n", maxend);
//     for (int j = 0; j < shop->njobs; j++) {
//         for (int o = 0; o < shop->nops; o++) {
//             // Output: start,duration,machine
//             fprintf(f, "%d,%d,%d ", shop->plan[j][o].stime, shop->plan[j][o].len, shop->plan[j][o].mach);
//         }
//         fprintf(f, "\n");
//     }
//     fclose(f);
// }

// int find_slot_par(ParallelShop *shop, int mach, int len, int estart) {
//     int st = estart;
//     while (1) {
//         int et = st + len, ok = 1, nextst = st;
//         for (int j = 0; j < shop->njobs; j++)
//             for (int o = 0; o < shop->nops; o++)
//                 if (shop->plan[j][o].stime != -1 && shop->plan[j][o].mach == mach) {
//                     int s2 = shop->plan[j][o].stime, e2 = s2 + shop->plan[j][o].len;
//                     if (st < e2 && et > s2) {
//                         ok = 0;
//                         if (e2 > nextst) nextst = e2;
//                     }
//                 }
//         if (ok) return st;
//         st = nextst;
//     }
// }

int parallel_schedule(ParallelShop *shop, int nth, int should_log) {
    int total = shop->njobs * shop->nops;
    int done[JMAX] = {0}, nextst[JMAX] = {0};
    int count = 0, iter = 0, maxit = total * 10;
    
    // Better load balancing: distribute jobs more evenly among threads
    int job_thread[JMAX];
    for (int j = 0; j < shop->njobs; j++) {
        job_thread[j] = j % nth;
    }
    
    while (count < total && iter < maxit) {
        iter++;
        int did = 0;
        
        #pragma omp parallel num_threads(nth) reduction(+:did)
        {
            int tid = omp_get_thread_num();
            int didlocal = 0;
            
            #pragma omp critical (sched)
            {
                // Process all jobs assigned to this thread
                for (int j = 0; j < shop->njobs; j++) {
                    if (job_thread[j] == tid && done[j] < shop->nops) {
                        int o = done[j];
                        int m = shop->plan[j][o].mach;
                        int l = shop->plan[j][o].len;
                        double t0 = omp_get_wtime();
                        int st = find_slot_par(shop, m, l, nextst[j]);
                        
                        // Only schedule if not already scheduled
                        if (shop->plan[j][o].stime == -1) {
                            shop->plan[j][o].stime = st;
                            done[j]++;
                            count++;
                            if (done[j] < shop->nops) nextst[j] = st + l;
                            
                            // Only log during the final measurement iteration
                            if (should_log) {
                                double t1 = omp_get_wtime();
                                int idx = shop->tlogc[tid];
                                if (idx < LOGMAX) {
                                    shop->tlogs[tid][idx].jid = j;
                                    shop->tlogs[tid][idx].oid = o;
                                    shop->tlogs[tid][idx].tstart = t0;
                                    shop->tlogs[tid][idx].tspan = t1 - t0;
                                    shop->tlogc[tid]++;
                                }
                            }
                            didlocal = 1; // Mark that this thread did work
                            break; // Process one operation per iteration per thread
                        }
                    }
                }
            }
            
            did += didlocal;
        }
        
        if (did == 0) break; // No progress possible
    }
    
    return (count == total);
}

// void dump_logs_par(ParallelShop *shop, int nth, const char *basename) {
//     const char* algorithm = "Greedy";
//     const char* size_category = get_size_category(shop->njobs, shop->nmachs);
    
//     char tfile[512], sfile[512], dir_path[512];
    
//     // Create directory structure
//     sprintf(dir_path, "../../Logs/%s/%s", algorithm, size_category);
// #ifdef _WIN32
//     char cmd[1024];
//     sprintf(cmd, "mkdir \"%s\" 2>nul", dir_path);
//     system(cmd);
// #else
//     char cmd[1024];
//     sprintf(cmd, "mkdir -p \"%s\"", dir_path);
//     system(cmd);
// #endif
    
//     char log_suffix_timing[100];
//     sprintf(log_suffix_timing, "timing_pargreedy_%d", nth);
//     get_log_path(tfile, algorithm, size_category, basename, log_suffix_timing); // Use common

//     char log_suffix_sequence[100];
//     sprintf(log_suffix_sequence, "sequence_pargreedy_%d", nth);
//     get_log_path(sfile, algorithm, size_category, basename, log_suffix_sequence); // Use common
    
//     FILE *ft = fopen(tfile, "w");
//     if (!ft) return;
//     fprintf(ft, "Thread | Ops | Total(s) | Avg(s)\n");
//     fprintf(ft, "-------------------------------\n");
//     for (int t = 0; t < nth; t++) {
//         double ttot = 0.0;
//         int cnt = shop->tlogc[t];
//         for (int i = 0; i < cnt; i++) ttot += shop->tlogs[t][i].tspan;
//         double avg = (cnt > 0) ? (ttot/cnt) : 0.0;
//         fprintf(ft, "%6d | %3d | %8.6f | %8.6f\n", t, cnt, ttot, avg);
//     }
//     fclose(ft);
//     FILE *fs = fopen(sfile, "w");
//     if (!fs) return;
//     fprintf(fs, "Thread | Job | Op | Time(s)\n");
//     fprintf(fs, "-----------------------------\n");
//     for (int t = 0; t < nth; t++)
//         for (int i = 0; i < shop->tlogc[t]; i++)
//             fprintf(fs, "%6d | %3d | %2d | %0.8f\n", t, shop->tlogs[t][i].jid, shop->tlogs[t][i].oid, shop->tlogs[t][i].tspan);
//     fclose(fs);
// }

int main(int argc, char *argv[]) {
    if (argc != 4) return 1;
    char iname[256], oname[256];
    int nth = atoi(argv[3]);
    if (nth < 1) nth = 1;
    strcpy(iname, argv[1]);
    strcpy(oname, argv[2]);
    
    char base[256];
    // Corrected call: extract_basename returns char*.
    char* temp_base_ptr = extract_basename(iname);
    if (temp_base_ptr) {
        strncpy(base, temp_base_ptr, sizeof(base) - 1);
        base[sizeof(base) - 1] = '\0'; // Ensure null termination
    } else {
        // Handle error or set a default basename
        strcpy(base, "unknown_input"); 
    }

    ParallelShop shop_instance; // Renamed to avoid potential conflicts
    if (!load_problem_par(iname, &shop_instance)) return 1; // Use shop_instance
    
    const char* algorithm = "Greedy"; // Algorithm name is specific here
    const char* size_category = get_size_category(shop_instance.njobs, shop_instance.nmachs); // Use common, use shop_instance
    
    char auto_result_path[512];
    char res_suffix[100];
    sprintf(res_suffix, "greedy_par_%d", nth);
    
    // Corrected call: get_result_path expects 4 arguments.
    // The suffix needs to be part of the basename if the common function doesn't take a suffix.
    char qualified_basename[356]; // Enough space for base + _ + res_suffix
    sprintf(qualified_basename, "%s_%s", base, res_suffix); // Create a basename that includes the parallel info
    get_result_path(auto_result_path, algorithm, size_category, qualified_basename);

    int total_ops = shop_instance.njobs * shop_instance.nops; // Use shop_instance
    int nthr_to_use = nth; // Renamed to avoid confusion
    if (nthr_to_use > total_ops) nthr_to_use = total_ops;
    if (nthr_to_use > 8 && total_ops < 100) nthr_to_use = 8; // Heuristic for thread count
    if (nthr_to_use < 1) nthr_to_use = 1;
    
    const int reps = 10000; 
    double ttot_timing = 0.0; // Renamed
#ifdef _WIN32
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
#endif
    for (int i = 0; i < reps; i++) {
        reset_plan_par(&shop_instance); // Use common, use shop_instance
        for (int t = 0; t < nthr_to_use; t++) shop_instance.tlogc[t] = 0; 
#ifdef _WIN32
        QueryPerformanceCounter(&t0);
        parallel_schedule(&shop_instance, nthr_to_use, (i == reps-1)); // Use shop_instance
        QueryPerformanceCounter(&t1);
        ttot_timing += (double)(t1.QuadPart - t0.QuadPart) / freq.QuadPart;
#else
        double t0 = omp_get_wtime();
        parallel_schedule(&shop_instance, nthr_to_use, (i == reps-1)); // Use shop_instance
        double t1 = omp_get_wtime();
        ttot_timing += t1 - t0;
#endif
    }
    double avg_timing = ttot_timing/reps; // Renamed
    
    dump_logs_par(&shop_instance, nthr_to_use, base); // Use shop_instance
    
    save_result_par(oname, &shop_instance); // Use shop_instance
    save_result_par(auto_result_path, &shop_instance); // Use shop_instance
    
    char sumfile[512], dir_path_log[512]; // Renamed
    sprintf(dir_path_log, "../../Logs/%s/%s", algorithm, size_category); // Renamed
#ifdef _WIN32
    char cmd_mkdir[1024]; // Renamed
    sprintf(cmd_mkdir, "mkdir \"%s\" 2>nul", dir_path_log); // Renamed
    system(cmd_mkdir); // Renamed
#else
    char cmd_mkdir_linux[1024]; // Renamed
    sprintf(cmd_mkdir_linux, "mkdir -p \"%s\"", dir_path_log); // Renamed
    system(cmd_mkdir_linux); // Renamed
#endif
    char exec_log_suffix[100];
    sprintf(exec_log_suffix, "exec_pargreedy_%d", nthr_to_use); // Suffix for summary log
    get_log_path(sumfile, algorithm, size_category, base, exec_log_suffix); // Use common for summary file path

    FILE *fsum = fopen(sumfile, "a");
    if (fsum) {
        fprintf(fsum, "Input: %s, Threads: %d, ParGreedy, AvgTime: %.9f s\n", base, nthr_to_use, avg_timing); // Renamed
        fclose(fsum);
    }
    return 0;
}
