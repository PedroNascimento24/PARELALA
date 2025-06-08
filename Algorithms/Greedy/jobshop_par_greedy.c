// jobshop_par_greedy.c
// Parallel job shop scheduler using parallel greedy heuristic
#include "../../Common/jobshop_common.h"
#include <omp.h>

// Helper functions (shared with sequential version)
const char* get_size_category(int njobs, int nmachs) {
    if (njobs <= 3 && nmachs <= 3) return "P1_Small";       // 3x3 = 9 ops
    else if (njobs <= 6 && nmachs <= 6) return "P2_Medium"; // 6x6 = 36 ops  
    else if (njobs <= 25 && nmachs <= 25) return "P3_Large"; // 25x25 = 625 ops
    else if (njobs <= 50 && nmachs <= 20) return "P4_XLarge"; // 50x20 = 1000 ops
    else return "P5_XXLarge"; // 100x20 = 2000 ops
}

void get_log_path_par(char* path, const char* algorithm, const char* size_category, const char* basename, const char* suffix, int nth) {
    sprintf(path, "../../Logs/%s/%s/%s_%s_%d.txt", algorithm, size_category, basename, suffix, nth);
}

void get_result_path_par(char* path, const char* algorithm, const char* size_category, const char* basename, int nth) {
    sprintf(path, "../../Result/%s/%s/%s_greedy_par_%d.txt", algorithm, size_category, basename, nth);
}

int load_problem_par(const char *fname, ParallelShop *shop) {
    FILE *f = fopen(fname, "r");
    if (!f) return 0;
    int r = fscanf(f, "%d %d", &shop->njobs, &shop->nmachs);
    if (r != 2) { fclose(f); return 0; }
    shop->nops = shop->nmachs;
    
    // Read problem data into static arrays
    for (int j = 0; j < shop->njobs; j++) {
        for (int o = 0; o < shop->nops; o++) {
            int rr = fscanf(f, "%d %d", &shop->plan[j][o].mach, &shop->plan[j][o].len);
            if (rr != 2) { fclose(f); return 0; }
            shop->plan[j][o].stime = -1;
        }
    }
    
    // Initialize thread log counters
    for (int t = 0; t < TMAX; t++) shop->tlogc[t] = 0;
    fclose(f);
    return 1;
}



void save_result_par(const char *fname, ParallelShop *shop) {
    // Create directory structure first
    char dir_path[512];
    char *last_slash = strrchr(fname, '/');
    char *last_bslash = strrchr(fname, '\\');
    char *last_sep = (last_slash > last_bslash) ? last_slash : last_bslash;
    
    if (last_sep) {
        strncpy(dir_path, fname, last_sep - fname);
        dir_path[last_sep - fname] = '\0';
        
#ifdef _WIN32
        char cmd[1024];
        sprintf(cmd, "mkdir \"%s\" 2>nul", dir_path);
        system(cmd);
#else
        char cmd[1024];
        sprintf(cmd, "mkdir -p \"%s\"", dir_path);
        system(cmd);
#endif
    }
    
    FILE *f = fopen(fname, "w");
    if (!f) return;
    int maxend = 0;
    for (int j = 0; j < shop->njobs; j++)
        for (int o = 0; o < shop->nops; o++) {
            int e = shop->plan[j][o].stime + shop->plan[j][o].len;
            if (e > maxend) maxend = e;
        }
    fprintf(f, "%d\n", maxend);
    for (int j = 0; j < shop->njobs; j++) {
        for (int o = 0; o < shop->nops; o++) {
            // Output: start,duration,machine
            fprintf(f, "%d,%d,%d ", shop->plan[j][o].stime, shop->plan[j][o].len, shop->plan[j][o].mach);
        }
        fprintf(f, "\n");
    }
    fclose(f);
}

int find_slot_par(ParallelShop *shop, int mach, int len, int estart) {
    int st = estart;
    while (1) {
        int et = st + len, ok = 1, nextst = st;
        for (int j = 0; j < shop->njobs; j++)
            for (int o = 0; o < shop->nops; o++)
                if (shop->plan[j][o].stime != -1 && shop->plan[j][o].mach == mach) {
                    int s2 = shop->plan[j][o].stime, e2 = s2 + shop->plan[j][o].len;
                    if (st < e2 && et > s2) {
                        ok = 0;
                        if (e2 > nextst) nextst = e2;
                    }
                }
        if (ok) return st;
        st = nextst;
    }
}

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

void reset_plan_par(ParallelShop *shop) {
    for (int j = 0; j < shop->njobs; j++)
        for (int o = 0; o < shop->nops; o++)
            shop->plan[j][o].stime = -1;
}

void dump_logs_par(ParallelShop *shop, int nth, const char *basename) {
    const char* algorithm = "Greedy";
    const char* size_category = get_size_category(shop->njobs, shop->nmachs);
    
    char tfile[512], sfile[512], dir_path[512];
    
    // Create directory structure
    sprintf(dir_path, "../../Logs/%s/%s", algorithm, size_category);
#ifdef _WIN32
    char cmd[1024];
    sprintf(cmd, "mkdir \"%s\" 2>nul", dir_path);
    system(cmd);
#else
    char cmd[1024];
    sprintf(cmd, "mkdir -p \"%s\"", dir_path);
    system(cmd);
#endif
    
    get_log_path_par(tfile, algorithm, size_category, basename, "timing_pargreedy", nth);
    get_log_path_par(sfile, algorithm, size_category, basename, "sequence_pargreedy", nth);
    
    FILE *ft = fopen(tfile, "w");
    if (!ft) return;
    fprintf(ft, "Thread | Ops | Total(s) | Avg(s)\n");
    fprintf(ft, "-------------------------------\n");
    for (int t = 0; t < nth; t++) {
        double ttot = 0.0;
        int cnt = shop->tlogc[t];
        for (int i = 0; i < cnt; i++) ttot += shop->tlogs[t][i].tspan;
        double avg = (cnt > 0) ? (ttot/cnt) : 0.0;
        fprintf(ft, "%6d | %3d | %8.6f | %8.6f\n", t, cnt, ttot, avg);
    }
    fclose(ft);
    FILE *fs = fopen(sfile, "w");
    if (!fs) return;
    fprintf(fs, "Thread | Job | Op | Time(s)\n");
    fprintf(fs, "-----------------------------\n");
    for (int t = 0; t < nth; t++)
        for (int i = 0; i < shop->tlogc[t]; i++)
            fprintf(fs, "%6d | %3d | %2d | %0.8f\n", t, shop->tlogs[t][i].jid, shop->tlogs[t][i].oid, shop->tlogs[t][i].tspan);
    fclose(fs);
}

int main(int argc, char *argv[]) {
    if (argc != 4) return 1;
    char iname[256], oname[256];
    int nth = atoi(argv[3]);
    if (nth < 1) nth = 1;
    strcpy(iname, argv[1]);
    strcpy(oname, argv[2]);
    char base[256] = {0};
    char *slash = strrchr(iname, '/');
    char *bslash = strrchr(iname, '\\');
    char *fname = iname;
    if (slash) fname = slash+1;
    else if (bslash) fname = bslash+1;
    strcpy(base, fname);
    char *dot = strrchr(base, '.');
    if (dot) *dot = 0;
    
    ParallelShop shop;
    if (!load_problem_par(iname, &shop)) return 1;
    
    // Auto-route output to appropriate folders based on problem size
    const char* algorithm = "Greedy";
    const char* size_category = get_size_category(shop.njobs, shop.nmachs);
    
    char auto_result_path[512];
    get_result_path_par(auto_result_path, algorithm, size_category, base, nth);
    
    int total = shop.njobs * shop.nops;
    int nthr = nth;
    if (nthr > total) nthr = total;
    if (nthr > 8 && total < 100) nthr = 8;
    if (nthr < 1) nthr = 1;
    
    const int reps = 10000; // Increase repetitions for better timing
    double ttot = 0.0;
#ifdef _WIN32
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
#endif
    for (int i = 0; i < reps; i++) {
        reset_plan_par(&shop);
        for (int t = 0; t < nthr; t++) shop.tlogc[t] = 0; // Always reset thread log counters
#ifdef _WIN32
        QueryPerformanceCounter(&t0);
        parallel_schedule(&shop, nthr, (i == reps-1)); // Only log on final iteration
        QueryPerformanceCounter(&t1);
        ttot += (double)(t1.QuadPart - t0.QuadPart) / freq.QuadPart;
#else
        double t0 = omp_get_wtime();
        parallel_schedule(&shop, nthr, (i == reps-1)); // Only log on final iteration
        double t1 = omp_get_wtime();
        ttot += t1 - t0;
#endif
    }
    double avg = ttot/reps;
    
    dump_logs_par(&shop, nthr, base);
    
    // Save to both user-specified path and auto-routed path
    save_result_par(oname, &shop);
    save_result_par(auto_result_path, &shop);
    
    char sumfile[512], dir_path[512];
    sprintf(dir_path, "../../Logs/%s/%s", algorithm, size_category);
#ifdef _WIN32
    char cmd[1024];
    sprintf(cmd, "mkdir \"%s\" 2>nul", dir_path);
    system(cmd);
#endif
    sprintf(sumfile, "../../Logs/%s/%s/%s_exec_pargreedy.txt", algorithm, size_category, base);
    FILE *fsum = fopen(sumfile, "a");
    if (fsum) {
        fprintf(fsum, "Input: %s, Threads: %d, ParGreedy, AvgTime: %.9f s\n", base, nthr, avg);
        fclose(fsum);
    }
    return 0;
}
