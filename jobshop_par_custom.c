// jobshop_par_custom.c
// Custom parallel job shop scheduler (logic equivalent, all new names)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <time.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif

#define JMAX 100
#define MMAX 100
#define OPMAX 100
#define LOGMAX 10000
#define TMAX 32

typedef struct {
    int mach;
    int len;
    int stime;
} Step;

typedef struct {
    int jid;
    int oid;
    double tstart;
    double tspan;
} ThreadLog;

typedef struct {
    int njobs;
    int nmachs;
    int nops;
    Step **plan; // dynamically allocated [njobs][nops]
    ThreadLog **tlogs; // dynamically allocated [TMAX][LOGMAX]
    int tlogc[TMAX];
} Shop;

void make_logs_dir() {
#ifdef _WIN32
    _mkdir("logs");
#else
    system("mkdir -p logs");
#endif
}

int load_problem(const char *fname, Shop *shop) {
    FILE *f = fopen(fname, "r");
    if (!f) return 0;
    int r = fscanf(f, "%d %d", &shop->njobs, &shop->nmachs);
    if (r != 2) { fclose(f); return 0; }
    shop->nops = shop->nmachs;
    // Allocate plan
    shop->plan = (Step**)malloc(shop->njobs * sizeof(Step*));
    for (int j = 0; j < shop->njobs; j++)
        shop->plan[j] = (Step*)malloc(shop->nops * sizeof(Step));
    // Allocate tlogs
    shop->tlogs = (ThreadLog**)malloc(TMAX * sizeof(ThreadLog*));
    for (int t = 0; t < TMAX; t++)
        shop->tlogs[t] = (ThreadLog*)malloc(LOGMAX * sizeof(ThreadLog));
    for (int j = 0; j < shop->njobs; j++) {
        for (int o = 0; o < shop->nops; o++) {
            int rr = fscanf(f, "%d %d", &shop->plan[j][o].mach, &shop->plan[j][o].len);
            if (rr != 2) { fclose(f); return 0; }
            shop->plan[j][o].stime = -1;
        }
    }
    for (int t = 0; t < TMAX; t++) shop->tlogc[t] = 0;
    fclose(f);
    return 1;
}

void free_shop(Shop *shop) {
    for (int j = 0; j < shop->njobs; j++) free(shop->plan[j]);
    free(shop->plan);
    for (int t = 0; t < TMAX; t++) free(shop->tlogs[t]);
    free(shop->tlogs);
}

void save_result(const char *fname, Shop *shop) {
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

int find_slot(Shop *shop, int mach, int len, int estart) {
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

int parallel_schedule(Shop *shop, int nth) {
    int total = shop->njobs * shop->nops;
    int done[JMAX] = {0}, nextst[JMAX] = {0}, assigned[JMAX][OPMAX], count = 0, iter = 0, maxit = total*10;
    for (int j = 0; j < shop->njobs; j++) {
        int t = j % nth;
        for (int o = 0; o < shop->nops; o++) assigned[j][o] = t;
    }
    int tworked[TMAX] = {0};
    while (count < total && iter < maxit) {
        iter++;
        int did = 0;
        #pragma omp parallel num_threads(nth) reduction(+:did)
        {
            int tid = omp_get_thread_num();
            int didlocal = 0;
            #pragma omp critical (sched)
            {
                for (int j = 0; j < shop->njobs; j++) {
                    if (assigned[j][0] == tid && done[j] < shop->nops) {
                        int o = done[j];
                        int m = shop->plan[j][o].mach;
                        int l = shop->plan[j][o].len;
                        double t0 = omp_get_wtime(); // Start timing before scheduling logic
                        int st = find_slot(shop, m, l, nextst[j]);
                        if (shop->plan[j][o].stime == -1) {
                            shop->plan[j][o].stime = st;
                            done[j]++;
                            count++;
                            if (done[j] < shop->nops) nextst[j] = st + l;
                            double t1 = omp_get_wtime(); // End timing after scheduling logic
                            int idx = shop->tlogc[tid];
                            if (idx < LOGMAX) {
                                shop->tlogs[tid][idx].jid = j;
                                shop->tlogs[tid][idx].oid = o;
                                shop->tlogs[tid][idx].tstart = t0;
                                shop->tlogs[tid][idx].tspan = t1-t0;
                                shop->tlogc[tid]++;
                            }
                            didlocal++;
                        }
                    }
                }
            }
            if (didlocal) tworked[tid] = 1;
            did += didlocal;
        }
        if (did == 0) {
            int all_assigned = 1;
            for (int j = 0; j < shop->njobs; j++) {
                if (done[j] < shop->nops) {
                    for (int t = 0; t < nth; t++) {
                        if (!tworked[t]) {
                            for (int o = done[j]; o < shop->nops; o++) assigned[j][o] = t;
                            tworked[t] = 1;
                            break;
                        }
                    }
                }
            }
            if (all_assigned) break;
        }
    }
    return (count == total);
}

void reset_plan(Shop *shop) {
    for (int j = 0; j < shop->njobs; j++)
        for (int o = 0; o < shop->nops; o++)
            shop->plan[j][o].stime = -1;
}

void dump_logs(Shop *shop, int nth, const char *basename) {
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
    Shop shop;
    if (!load_problem(iname, &shop)) return 1;
    make_logs_dir();
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
        reset_plan(&shop);
        if (i < reps-1) for (int t = 0; t < nthr; t++) shop.tlogc[t] = 0;
#ifdef _WIN32
        QueryPerformanceCounter(&t0);
        parallel_schedule(&shop, nthr);
        QueryPerformanceCounter(&t1);
        ttot += (double)(t1.QuadPart - t0.QuadPart) / freq.QuadPart;
#else
        double t0 = omp_get_wtime();
        parallel_schedule(&shop, nthr);
        double t1 = omp_get_wtime();
        ttot += t1 - t0;
#endif
    }
    double avg = ttot/reps;
    dump_logs(&shop, nthr, base);
    save_result(oname, &shop);
    char sumfile[256];
    sprintf(sumfile, "logs/%s_exec_parcustom.txt", base);
    FILE *fsum = fopen(sumfile, "a");
    if (fsum) {
        fprintf(fsum, "Input: %s, Threads: %d, ParCustom, AvgTime: %.9f s\n", base, nthr, avg);
        fclose(fsum);
    }
    free_shop(&shop);
    return 0;
}
