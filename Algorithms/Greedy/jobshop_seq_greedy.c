// jobshop_seq_greedy.c
// Sequential job shop scheduler using greedy earliest-start heuristic
#include "../../Common/jobshop_common.h"

// Determine size category based on problem dimensions
const char* get_size_category(int njobs, int nmachs) {
    if (njobs <= 3 && nmachs <= 3) return "P1_Small";       // 3x3 = 9 ops
    else if (njobs <= 6 && nmachs <= 6) return "P2_Medium"; // 6x6 = 36 ops  
    else if (njobs <= 25 && nmachs <= 25) return "P3_Large"; // 25x25 = 625 ops
    else if (njobs <= 50 && nmachs <= 20) return "P4_XLarge"; // 50x20 = 1000 ops
    else return "P5_XXLarge"; // 100x20 = 2000 ops
}

// Create algorithm-specific directory structure
void create_algorithm_dirs(const char* algorithm) {
    char path[512];
    
    // Create main Logs and Result directories
    sprintf(path, "../../Logs/%s", algorithm);
#ifdef _WIN32
    _mkdir("../../Logs");
    _mkdir(path);
#else
    system("mkdir -p ../../Logs");
    system("mkdir -p $path");
#endif

    sprintf(path, "../../Result/%s", algorithm);
#ifdef _WIN32
    _mkdir("../../Result");
    _mkdir(path);
#else
    system("mkdir -p ../../Result");
    system("mkdir -p $path");
#endif
}

// Generate log file path with algorithm and size organization
void get_log_path(char* path, const char* algorithm, const char* size_category, const char* basename, const char* suffix) {
    sprintf(path, "../../Logs/%s/%s/%s_%s.txt", algorithm, size_category, basename, suffix);
}

// Generate result file path with algorithm and size organization  
void get_result_path(char* path, const char* algorithm, const char* size_category, const char* basename) {
    sprintf(path, "../../Result/%s/%s/%s_greedy_seq.txt", algorithm, size_category, basename);
}

int load_problem_seq(const char *fname, Shop *shop) {
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

void save_result_seq(const char *fname, Shop *shop) {
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

int find_slot_seq(Shop *shop, int mach, int len, int estart) {
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
        int st = find_slot_seq(shop, m, l, nextst[bestj]);
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

void dump_logs_seq(Shop *shop, const char *basename) {
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
    
    get_log_path(tfile, algorithm, size_category, basename, "timing_seqgreedy");
    get_log_path(sfile, algorithm, size_category, basename, "sequence_seqgreedy");
    
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
    if (!load_problem_seq(iname, &shop)) return 1;
    
    // Auto-route output to appropriate folders based on problem size
    const char* algorithm = "Greedy";
    const char* size_category = get_size_category(shop.njobs, shop.nmachs);
    
    char auto_result_path[512];
    get_result_path(auto_result_path, algorithm, size_category, base);
    
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
    
    dump_logs_seq(&shop, base);
    
    // Save to both user-specified path and auto-routed path
    save_result_seq(oname, &shop);
    save_result_seq(auto_result_path, &shop);
    
    char sumfile[512], dir_path[512];
    sprintf(dir_path, "../../Logs/%s/%s", algorithm, size_category);
#ifdef _WIN32
    char cmd[1024];
    sprintf(cmd, "mkdir \"%s\" 2>nul", dir_path);
    system(cmd);
#endif
    sprintf(sumfile, "../../Logs/%s/%s/%s_exec_seqgreedy.txt", algorithm, size_category, base);
    FILE *fsum = fopen(sumfile, "a");
    if (fsum) {
        fprintf(fsum, "Input: %s, SeqGreedy, AvgTime: %.9f s\n", base, avg);
        fclose(fsum);
    }
    return 0;
}
