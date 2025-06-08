# Build script for all Job Shop Scheduling algorithms
# Compiles all available algorithms with proper error handling and progress reporting

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "=== BUILDING JOB SHOP ALGORITHMS ===" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# Check if GCC is available
Write-Host "Checking GCC compiler availability..." -ForegroundColor White
try {
    $gccVersion = gcc --version 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "SUCCESS: GCC compiler found" -ForegroundColor Green
    } else {
        Write-Host "ERROR: GCC compiler not found in PATH" -ForegroundColor Red
        Write-Host "Please install MinGW-w64 or ensure GCC is in your PATH" -ForegroundColor Red
        exit 1
    }
} catch {
    Write-Host "ERROR: GCC compiler not found" -ForegroundColor Red
    exit 1
}

# Clean up old executables
Write-Host "`nCleaning up old executables..." -ForegroundColor White
Get-ChildItem -Path "../Algorithms" -Filter "*.exe" -Recurse -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
Write-Host "SUCCESS: Old executables removed" -ForegroundColor Green

# Build counters
$totalBuilds = 5  # Greedy Sequential, Greedy Parallel, SPT, LPT, MOR
$currentBuild = 0
$successfulBuilds = 0
$failedBuilds = 0

Write-Host "`n=========================================" -ForegroundColor Magenta
Write-Host "=== BUILDING GREEDY ALGORITHMS ===" -ForegroundColor Magenta
Write-Host "=========================================" -ForegroundColor Magenta

# Build Greedy Sequential
$currentBuild++
Write-Host "`n[$currentBuild/$totalBuilds] Building Greedy Sequential Algorithm..." -ForegroundColor White
Push-Location "../Algorithms/Greedy"
$result = gcc -o jobshop_seq_greedy.exe jobshop_seq_greedy.c -std=c99 -O2 -Wall 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "SUCCESS: Greedy Sequential compiled successfully" -ForegroundColor Green
    $successfulBuilds++
} else {
    Write-Host "ERROR: Greedy Sequential compilation failed" -ForegroundColor Red
    Write-Host $result -ForegroundColor Red
    $failedBuilds++
}

# Build Greedy Parallel
$currentBuild++
Write-Host "`n[$currentBuild/$totalBuilds] Building Greedy Parallel Algorithm..." -ForegroundColor White
$result = gcc -fopenmp -o jobshop_par_greedy.exe jobshop_par_greedy.c -std=c99 -O2 -Wall 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "SUCCESS: Greedy Parallel compiled successfully" -ForegroundColor Green
    $successfulBuilds++
} else {
    Write-Host "ERROR: Greedy Parallel compilation failed" -ForegroundColor Red
    Write-Host $result -ForegroundColor Red
    $failedBuilds++
}

Pop-Location

Write-Host "`n=========================================" -ForegroundColor Magenta
Write-Host "=== BUILDING HEURISTIC ALGORITHMS ===" -ForegroundColor Magenta
Write-Host "=========================================" -ForegroundColor Magenta

# Build SPT Sequential
$currentBuild++
Write-Host "`n[$currentBuild/$totalBuilds] Building SPT Sequential Algorithm..." -ForegroundColor White
Push-Location "../Algorithms/SPT"
$result = gcc -o jobshop_seq_spt.exe jobshop_seq_spt.c -std=c99 -O2 -Wall 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "SUCCESS: SPT Sequential compiled successfully" -ForegroundColor Green
    $successfulBuilds++
} else {
    Write-Host "ERROR: SPT Sequential compilation failed" -ForegroundColor Red
    Write-Host $result -ForegroundColor Red
    $failedBuilds++
}
Pop-Location

# Build LPT Sequential
$currentBuild++
Write-Host "`n[$currentBuild/$totalBuilds] Building LPT Sequential Algorithm..." -ForegroundColor White
Push-Location "../Algorithms/LPT"
$result = gcc -o jobshop_seq_lpt.exe jobshop_seq_lpt.c -std=c99 -O2 -Wall 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "SUCCESS: LPT Sequential compiled successfully" -ForegroundColor Green
    $successfulBuilds++
} else {
    Write-Host "ERROR: LPT Sequential compilation failed" -ForegroundColor Red
    Write-Host $result -ForegroundColor Red
    $failedBuilds++
}
Pop-Location

# Build MOR Sequential
$currentBuild++
Write-Host "`n[$currentBuild/$totalBuilds] Building MOR Sequential Algorithm..." -ForegroundColor White
Push-Location "../Algorithms/MOR"
$result = gcc -o jobshop_seq_mor.exe jobshop_seq_mor.c -std=c99 -O2 -Wall 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Host "SUCCESS: MOR Sequential compiled successfully" -ForegroundColor Green
    $successfulBuilds++
} else {
    Write-Host "ERROR: MOR Sequential compilation failed" -ForegroundColor Red
    Write-Host $result -ForegroundColor Red
    $failedBuilds++
}
Pop-Location

# Future algorithms (placeholders for when implemented)
Write-Host "`n==========================================" -ForegroundColor Yellow
Write-Host "=== FUTURE ALGORITHMS (NOT IMPLEMENTED) ===" -ForegroundColor Yellow
Write-Host "==========================================" -ForegroundColor Yellow
Write-Host "-> Branch & Bound: Will be implemented in future versions" -ForegroundColor Gray
Write-Host "-> Shifting Bottleneck: Will be implemented in future versions" -ForegroundColor Gray

# Build Summary
Write-Host "`n==========================================" -ForegroundColor Cyan
Write-Host "=== BUILD SUMMARY ===" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

$successRate = if ($totalBuilds -gt 0) { [math]::Round(($successfulBuilds / $totalBuilds) * 100, 1) } else { 0 }

Write-Host "Build Results:" -ForegroundColor White
Write-Host "  -> Total builds: $totalBuilds" -ForegroundColor White
Write-Host "  -> Successful: $successfulBuilds" -ForegroundColor Green
Write-Host "  -> Failed: $failedBuilds" -ForegroundColor Red
Write-Host "  -> Success rate: $successRate%" -ForegroundColor White

# Verify executables exist
Write-Host "`nVerifying built executables:" -ForegroundColor White
$executables = @(
    @{Path="../Algorithms/Greedy/jobshop_seq_greedy.exe"; Name="Sequential Greedy"},
    @{Path="../Algorithms/Greedy/jobshop_par_greedy.exe"; Name="Parallel Greedy"},
    @{Path="../Algorithms/SPT/jobshop_seq_spt.exe"; Name="SPT Sequential"},
    @{Path="../Algorithms/LPT/jobshop_seq_lpt.exe"; Name="LPT Sequential"},
    @{Path="../Algorithms/MOR/jobshop_seq_mor.exe"; Name="MOR Sequential"}
)

foreach ($exe in $executables) {
    if (Test-Path $exe.Path) {
        $size = [math]::Round((Get-Item $exe.Path).Length / 1KB, 1)
        Write-Host "  -> SUCCESS: $($exe.Name): $($exe.Path) ($size KB)" -ForegroundColor Green
    } else {
        Write-Host "  -> ERROR: $($exe.Name): Missing!" -ForegroundColor Red
    }
}

# Final status
if ($failedBuilds -eq 0) {
    Write-Host "`nALL BUILDS SUCCESSFUL!" -ForegroundColor Green
    Write-Host "You can now run .\Scripts\ultimate_analysis.ps1 to execute comprehensive tests." -ForegroundColor Green
    exit 0
} elseif ($successfulBuilds -gt 0) {
    Write-Host "`nSOME BUILDS FAILED" -ForegroundColor Yellow
    Write-Host "Review compilation errors above. Some algorithms may still be testable." -ForegroundColor Yellow
    exit 1
} else {
    Write-Host "`nALL BUILDS FAILED" -ForegroundColor Red
    Write-Host "Check compilation errors above and verify source code integrity." -ForegroundColor Red
    exit 1
}
