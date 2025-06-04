// jobshop_seq_custom.c
// Custom sequential job shop scheduler (logic equivalent, all new names)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#endif

#define JMAX 100
#define MMAX 100
#define OPMAX 100
#define LOGMAX 10000

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
} LogEntry;

typedef struct {
    int njobs;
    int nmachs;
    int nops;
    Step plan[JMAX][OPMAX];
    LogEntry logs[LOGMAX];
    int nlogs;
} Shop;

void make_logs_dir() {
#ifdef _WIN32
    _mkdir("logs");
#else
    mkdir("logs", 0777);
#endif
}

int load_problem(const char *fname, Shop *shop) {
    FILE *f = fopen(fname, "r");
    if (!f) return 0;
    fscanf(f, "%d %d", &shop->njobs, &shop->nmachs);
    shop->nops = shop->nmachs;
    shop->nlogs = 0;
    for (int j = 0; j < shop->njobs; j++) {
        for (int o = 0; o < shop->nops; o++) {
            fscanf(f, "%d %d", &shop->plan[j][o].mach, &shop->plan[j][o].len);
            shop->plan[j][o].stime = -1;
        }
    }
    fclose(f);
    return 1;
}

void save_result(const char *fname, Shop *shop) {
    FILE *f = fopen(fname, "w");
    if (!f) return;
    int maxend = 0;
    for (int j = 0; j < shop->njobs; j++) {
        for (int o = 0; o < shop->nops; o++) {
            int e = shop->plan[j][o].stime + shop->plan[j][o].len;
            if (e > maxend) maxend = e;
        }
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
        int et = st + len;
        int ok = 1, nextst = st;
        for (int j = 0; j < shop->njobs; j++) {
            for (int o = 0; o < shop->nops; o++) {
                if (shop->plan[j][o].stime != -1 && shop->plan[j][o].mach == mach) {
                    int s2 = shop->plan[j][o].stime;
                    int e2 = s2 + shop->plan[j][o].len;
                    if (st < e2 && et > s2) {
                        ok = 0;
                        if (e2 > nextst) nextst = e2;
                    }
                }
            }
        }
        if (ok) return st;
        st = nextst;
    }
}

void greedy_schedule(Shop *shop) {
    int done[JMAX] = {0};
    int nextst[JMAX] = {0};
    int total = shop->njobs * shop->nops, count = 0;
    while (count < total) {
        int bestj = -1, bestt = 999999;
        for (int j = 0; j < shop->njobs; j++) {
            if (done[j] < shop->nops && nextst[j] < bestt) {
                bestt = nextst[j];
                bestj = j;
            }
        }
        if (bestj == -1) break;
        int o = done[bestj];
        int m = shop->plan[bestj][o].mach;
        int l = shop->plan[bestj][o].len;
        clock_t t0 = clock(); // Start timing before scheduling logic
        int st = find_slot(shop, m, l, nextst[bestj]);
        shop->plan[bestj][o].stime = st;
        done[bestj]++;
        count++;
        if (done[bestj] < shop->nops) nextst[bestj] = st + l;
        clock_t t1 = clock(); // End timing after scheduling logic
        double dt = ((double)(t1-t0))/CLOCKS_PER_SEC;
        if (shop->nlogs < LOGMAX) {
            shop->logs[shop->nlogs].jid = bestj;
            shop->logs[shop->nlogs].oid = o;
            shop->logs[shop->nlogs].tstart = ((double)t0)/CLOCKS_PER_SEC;
            shop->logs[shop->nlogs].tspan = dt;
            shop->nlogs++;
        }
    }
}

void dump_logs(Shop *shop, const char *basename) {
    make_logs_dir();
    char tfile[256], sfile[256];
    sprintf(tfile, "logs/%s_timing_seqcustom.txt", basename);
    sprintf(sfile, "logs/%s_sequence_seqcustom.txt", basename);
    FILE *ft = fopen(tfile, "w");
    if (!ft) return;
    fprintf(ft, "TotalOps | TotalTime(s) | AvgTimePerOp(s)\n");
    fprintf(ft, "------------------------------------------\n");
    double ttot = 0.0;
    for (int i = 0; i < shop->nlogs; i++) ttot += shop->logs[i].tspan;
    double avg = (shop->nlogs > 0) ? (ttot/shop->nlogs) : 0.0;
    fprintf(ft, "%8d | %12.8f | %15.8f\n", shop->nlogs, ttot, avg);
    fclose(ft);
    FILE *fs = fopen(sfile, "w");
    if (!fs) return;
    fprintf(fs, "Order | Job | Op | Time(s)\n");
    fprintf(fs, "-----------------------------\n");
    for (int i = 0; i < shop->nlogs; i++) {
        fprintf(fs, "%5d | %3d | %2d | %0.8f\n", i+1, shop->logs[i].jid, shop->logs[i].oid, shop->logs[i].tspan);
    }
    fclose(fs);
}

int main(int argc, char *argv[]) {
    if (argc != 3) return 1;
    char iname[256], oname[256];
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
    const int reps = 10000; // Increase repetitions for better timing
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    double ttot = 0.0;
    for (int i = 0; i < reps; i++) {
        for (int j = 0; j < shop.njobs; j++)
            for (int o = 0; o < shop.nops; o++)
                shop.plan[j][o].stime = -1;
        if (i < reps-1) shop.nlogs = 0;
        QueryPerformanceCounter(&t0);
        greedy_schedule(&shop);
        QueryPerformanceCounter(&t1);
        ttot += (double)(t1.QuadPart - t0.QuadPart) / freq.QuadPart;
    }
    double avg = ttot/reps;
    dump_logs(&shop, base);
    save_result(oname, &shop);
    char sumfile[256];
    sprintf(sumfile, "logs/%s_exec_seqcustom.txt", base);
    FILE *fsum = fopen(sumfile, "a");
    if (fsum) {
        fprintf(fsum, "Input: %s, SeqCustom, AvgTime: %.9f s\n", base, avg);
        fclose(fsum);
    }
    return 0;
}
