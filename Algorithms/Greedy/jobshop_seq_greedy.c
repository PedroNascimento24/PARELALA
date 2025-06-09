// jobshop_seq_greedy.c
// Sequential job shop scheduler using greedy earliest-start heuristic
#include "../../Common/jobshop_common.h"

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
    
    Shop shop_instance; // Renamed to avoid conflict with global 'shop' if any after includes
    if (!load_problem_seq(iname, &shop_instance)) return 1;
    
    // Auto-route output to appropriate folders based on problem size
    const char* algorithm = "Greedy"; // This should be specific to the algorithm file
    const char* size_category = get_size_category(shop_instance.njobs, shop_instance.nmachs);
    
    char auto_result_path[512];
    // Corrected call: get_result_path expects 4 arguments.
    // The "seq" or other algorithm-specific parts should ideally be part of the 'basename'
    // or handled by the common function if it's designed that way.
    // For now, adhering strictly to the 4-argument signature.
    // If "Greedy_seq" is desired in the filename, 'base' might need to be "1_Small_sample_Greedy_seq"
    // or the common function needs to append the algorithm name.
    // Let's assume 'base' is just the problem name, and common get_result_path uses 'algorithm' parameter.
    get_result_path(auto_result_path, algorithm, size_category, base);
    
    const int reps = 10000; // Increase repetitions for better timing
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    double ttot_timing = 0.0; // Renamed to avoid conflict
    for (int i = 0; i < reps; i++) {
        for (int j = 0; j < shop_instance.njobs; j++)
            for (int o = 0; o < shop_instance.nops; o++)
                shop_instance.plan[j][o].stime = -1;
        if (i < reps-1) shop_instance.nlogs = 0; // Use shop_instance
        QueryPerformanceCounter(&t0);
        greedy_schedule(&shop_instance); // Use shop_instance
        QueryPerformanceCounter(&t1);
        ttot_timing += (double)(t1.QuadPart - t0.QuadPart) / freq.QuadPart; // Use ttot_timing
    }
    double avg_timing = ttot_timing/reps; // Use avg_timing and ttot_timing
    
    // Corrected call: dump_logs_seq expects 2 arguments.
    dump_logs_seq(&shop_instance, base);
    
    // Save to both user-specified path and auto-routed path
    save_result_seq(oname, &shop_instance); // Use shop_instance
    save_result_seq(auto_result_path, &shop_instance); // Use shop_instance
    
    char sumfile[512], dir_path_log[512]; // Renamed dir_path to dir_path_log
    sprintf(dir_path_log, "../../Logs/%s/%s", algorithm, size_category); // Use dir_path_log
#ifdef _WIN32
    char cmd_mkdir[1024]; // Renamed cmd to cmd_mkdir
    sprintf(cmd_mkdir, "mkdir \"%s\" 2>nul", dir_path_log); // Use cmd_mkdir and dir_path_log
    system(cmd_mkdir); // Use cmd_mkdir
#else
    char cmd_mkdir_linux[1024]; // Renamed cmd for Linux
    sprintf(cmd_mkdir_linux, "mkdir -p \"%s\"", dir_path_log); // Use cmd_mkdir_linux and dir_path_log
    system(cmd_mkdir_linux); // Use cmd_mkdir_linux
#endif
    // Use get_log_path for consistency in summary file path generation
    get_log_path(sumfile, algorithm, size_category, base, "exec_seqgreedy");

    FILE *fsum = fopen(sumfile, "a");
    if (fsum) {
        fprintf(fsum, "Input: %s, SeqGreedy, AvgTime: %.9f s\n", base, avg_timing); // Use avg_timing
        fclose(fsum);
    }
    return 0;
}
