// jobshop_par_custom.c
// Custom parallel job shop scheduler using parallel greedy heuristic
#include "jobshop_common.h"
#include <omp.h>

void make_logs_dir() {
#ifdef _WIN32
    _mkdir("logs");
#else
    system("mkdir -p logs");
#endif
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
    make_logs_dir();
    char tfile[256], sfile[256];
    sprintf(tfile, "logs/%s_timing_parcustom_%d.txt", basename, nth);
    sprintf(sfile, "logs/%s_sequence_parcustom_%d.txt", basename, nth);
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
    make_logs_dir();
    int total = shop.njobs * shop.nops;
    int nthr = nth;
    if (nthr > total) nthr = total;
    if (nthr > 8 && total < 100) nthr = 8;
    if (nthr < 1) nthr = 1;    const int reps = 10000; // Increase repetitions for better timing
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
    save_result_par(oname, &shop);
    char sumfile[256];
    sprintf(sumfile, "logs/%s_exec_parcustom.txt", base);
    FILE *fsum = fopen(sumfile, "a");
    if (fsum) {
        fprintf(fsum, "Input: %s, Threads: %d, ParCustom, AvgTime: %.9f s\n", base, nthr, avg);
        fclose(fsum);
    }
    return 0;
}
