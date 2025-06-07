# PowerShell script to clean, build, and run all Job Shop experiments
# 1. Delete all .exe and .txt output/log files
# 2. Compile C programs
# 3. Run all experiments on all datasets

# --- 1. Clean up ---
Write-Host "Cleaning up .exe and output/log .txt files..."

# Remove .exe files in root
get-childitem -Path . -Filter *.exe | remove-item -Force -ErrorAction SilentlyContinue

# Remove output .txt files in Output/
if (Test-Path Output) {
    get-childitem -Path Output -Filter *.txt | remove-item -Force -ErrorAction SilentlyContinue
}

# Remove log .txt files in logs/
if (Test-Path logs) {
    get-childitem -Path logs -Filter *.txt | remove-item -Force -ErrorAction SilentlyContinue
}

# --- 2. Compile C programs ---
Write-Host "Compiling C programs..."

gcc jobshop_seq_custom.c -o jobshop_seq_custom.exe
if ($LASTEXITCODE -ne 0) { Write-Host "Failed to compile jobshop_seq_custom.c"; exit 1 }
gcc jobshop_par_custom.c -fopenmp -o jobshop_par_custom.exe
if ($LASTEXITCODE -ne 0) { Write-Host "Failed to compile jobshop_par_custom.c"; exit 1 }

# --- 3. Run all experiments ---
Write-Host "Running all experiments..."

# Sequential runs for all datasets (guaranteed, separate commands)
Write-Host "Running sequential: Data/Small_sample.jss -> Output/Small_seqcustom.txt"
.\jobshop_seq_custom.exe Data/Small_sample.jss Output/Small_seqcustom.txt
Write-Host "Running sequential: Data/Medium_sample.jss -> Output/Medium_seqcustom.txt"
.\jobshop_seq_custom.exe Data/Medium_sample.jss Output/Medium_seqcustom.txt
Write-Host "Running sequential: Data/Big_sample.jss -> Output/Big_seqcustom.txt"
.\jobshop_seq_custom.exe Data/Big_sample.jss Output/Big_seqcustom.txt

# Parallel runs for Small and Medium (16 threads, guaranteed, separate commands)
Write-Host "Running parallel (16 threads): Data/Small_sample.jss -> Output/Small_parcustom_16.txt"
.\jobshop_par_custom.exe Data/Small_sample.jss Output/Small_parcustom_16.txt 16
Write-Host "Running parallel (16 threads): Data/Medium_sample.jss -> Output/Medium_parcustom_16.txt"
.\jobshop_par_custom.exe Data/Medium_sample.jss Output/Medium_parcustom_16.txt 16

# Parallel runs for Big with multiple thread counts (guaranteed, separate commands)
Write-Host "Running parallel: Data/Big_sample.jss -> Output/Big_parcustom_1.txt (Threads: 1)"
.\jobshop_par_custom.exe Data/Big_sample.jss Output/Big_parcustom_1.txt 1
Write-Host "Running parallel: Data/Big_sample.jss -> Output/Big_parcustom_2.txt (Threads: 2)"
.\jobshop_par_custom.exe Data/Big_sample.jss Output/Big_parcustom_2.txt 2
Write-Host "Running parallel: Data/Big_sample.jss -> Output/Big_parcustom_4.txt (Threads: 4)"
#.\jobshop_par_custom.exe Data/Big_sample.jss Output/Big_parcustom_4.txt 4
Write-Host "Running parallel: Data/Big_sample.jss -> Output/Big_parcustom_8.txt (Threads: 8)"
#.\jobshop_par_custom.exe Data/Big_sample.jss Output/Big_parcustom_8.txt 8
Write-Host "Running parallel: Data/Big_sample.jss -> Output/Big_parcustom_16.txt (Threads: 16)"
#.\jobshop_par_custom.exe Data/Big_sample.jss Output/Big_parcustom_16.txt 16
Write-Host "Running parallel: Data/Big_sample.jss -> Output/Big_parcustom_32.txt (Threads: 32)"
#.\jobshop_par_custom.exe Data/Big_sample.jss Output/Big_parcustom_32.txt 32

Write-Host "All done!"
