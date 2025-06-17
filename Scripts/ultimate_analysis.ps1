# =============================================================================
# ULTIMATE JOB SHOP SCHEDULING ANALYSIS SUITE
# =============================================================================
# ONE SCRIPT TO RULE THEM ALL - Complete analysis solution with full configurability
# Replaces: run_all.ps1, analyze_results.ps1, comprehensive_analysis.ps1, test_comprehensive.ps1
# Features: Algorithm testing, performance analysis, makespan optimization, configurable settings

param(
    [string]$ConfigFile = "analysis_config.json",
    [switch]$QuickTest,
    [switch]$GenerateConfig,
    [switch]$CleanOnly,
    [ValidateSet("Greedy", "ShiftingBottleneck", "BranchAndBound", "")]
    [string]$AlgorithmFilter = "",
    [ValidateSet("Small", "Medium", "Large", "XLarge", "XXLarge", "XXXLarge", "P1_Small", "P2_Medium", "P3_Large", "P4_XLarge", "P5_XXLarge", "P6_XXXLarge", "")]
    [string]$DatasetFilter = ""
)

Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "=== ULTIMATE JOBSHOP ANALYSIS SUITE ===" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan

# Enhanced default configuration with ALL algorithms and flexible settings
$defaultConfig = @{
    datasets   = @(
        @{
            ExpectedTime = "<100ms"
            Category     = "P1_Small"
            Name         = "1_Small_sample"
            Size         = "3x3"
        },
        @{
            ExpectedTime = "<200ms"
            Category     = "P2_Medium"
            Name         = "2_Medium_sample"
            Size         = "6x6"
        },
        @{
            ExpectedTime = "<5s"
            Category     = "P3_Large"
            Name         = "3_Big_sample"
            Size         = "25x25"
        },
        @{
            ExpectedTime = "<30s"
            Category     = "P4_XLarge"
            Name         = "4_XLarge_sample"
            Size         = "50x20"
        },
        @{
            ExpectedTime = "<2min"
            Category     = "P5_XXLarge"
            Name         = "5_XXLarge_sample"
            Size         = "100x20"
        },
        @{
            ExpectedTime = "<10min"
            Category     = "P6_XXXLarge"
            Name         = "6_XXXLarge_sample"
            Size         = "100x100"
        }
    )
    algorithms = @{
        Greedy = @{
            description = "Earliest start time greedy heuristic"
            sequential  = @{
                executable = "..\\\\Algorithms\\\\Greedy\\\\jobshop_seq_greedy.exe"
                enabled    = $false 
            }
            parallel    = @{
                threadCounts = @(1, 2, 4, 8, 16)
                executable   = "..\\\\Algorithms\\\\Greedy\\\\jobshop_par_greedy.exe"
                enabled      = $false
            }
        }
        BranchAndBound = @{
            description = "Branch & Bound optimal search algorithm"
            sequential  = @{
                executable = "..\\\\Algorithms\\\\BranchAndBound\\\\jobshop_seq_bb.exe"
                enabled    = $true 
            }
            parallel    = @{
                threadCounts = @(1, 2, 4, 8, 16)
                executable   = "..\\\\Algorithms\\\\BranchAndBound\\\\jobshop_par_bb.exe"
                enabled      = $true
            }
        }
        ShiftingBottleneck = @{
            description = "Shifting Bottleneck heuristic"
            sequential  = @{
                executable = "..\\\\Algorithms\\\\ShiftingBottleneck\\\\jobshop_seq_sb.exe"
                enabled    = $true
            }
            parallel    = @{
                threadCounts = @(1, 2, 4, 8, 16)
                executable   = "..\\\\Algorithms\\\\ShiftingBottleneck\\\\jobshop_par_sb.exe"
                enabled      = $true
            }
        }
    }
    settings   = @{
        verbose         = $true
        repetitions     = 10000 # Default repetitions, can be overridden by config file
        generateReports = $true
        cleanBefore     = $true
    }
}

# Handle special modes first
if ($GenerateConfig) {
    Write-Host "Generating fresh configuration file: $ConfigFile" -ForegroundColor Yellow
    $defaultConfig | ConvertTo-Json -Depth 10 | Out-File $ConfigFile -Encoding UTF8
    Write-Host "SUCCESS: Configuration file created!" -ForegroundColor Green
    Write-Host "Edit $ConfigFile to customize settings" -ForegroundColor White
    Write-Host "`nExample usage:" -ForegroundColor Gray
    Write-Host "  .\ultimate_analysis.ps1                    # Run all enabled algorithms" -ForegroundColor Gray
    Write-Host "  .\ultimate_analysis.ps1 -QuickTest         # Quick test with small dataset only" -ForegroundColor Gray
    Write-Host "  .\ultimate_analysis.ps1 -AlgorithmFilter SPT # Run only SPT algorithm" -ForegroundColor Gray
    Write-Host "  .\ultimate_analysis.ps1 -DatasetFilter Small # Run only small dataset" -ForegroundColor Gray
    Write-Host "  .\ultimate_analysis.ps1 -CleanOnly         # Clean all previous results" -ForegroundColor Gray
    exit 0
}

#if ($CleanOnly) {
#    Write-Host "=== CLEANING MODE ===" -ForegroundColor Red
#    Get-ChildItem -Path "..\\Result" -Filter "*.txt" -Recurse -ErrorAction SilentlyContinue | Remove-Item -Force
#    Get-ChildItem -Path ".\\Logs" -Filter "*.txt" -Recurse -ErrorAction SilentlyContinue | Remove-Item -Force
#    Remove-Item "comprehensive_analysis_report.txt" -ErrorAction SilentlyContinue
#    Write-Host "SUCCESS: All previous results cleaned" -ForegroundColor Green
#    exit 0
#}

# Load or create configuration
if (Test-Path $ConfigFile) {
    Write-Host "Loading configuration from: $ConfigFile" -ForegroundColor White
    $config = Get-Content $ConfigFile | ConvertFrom-Json
}
else {
    Write-Host "Creating default configuration: $ConfigFile" -ForegroundColor Yellow
    $config = $defaultConfig
    $config | ConvertTo-Json -Depth 10 | Out-File $ConfigFile -Encoding UTF8
    Write-Host "Edit $ConfigFile to customize settings and algorithms" -ForegroundColor Yellow
    Write-Host "Use -GenerateConfig to create fresh config" -ForegroundColor Gray
}

# Apply filters if specified
if ($AlgorithmFilter) {
    Write-Host "Filtering algorithms: $AlgorithmFilter" -ForegroundColor Yellow
    $filteredAlgorithms = [PSCustomObject]@{}
    foreach ($algName in $config.algorithms.PSObject.Properties.Name) {
        if ($algName -like "*$AlgorithmFilter*") {
            $filteredAlgorithms | Add-Member -MemberType NoteProperty -Name $algName -Value $config.algorithms.$algName
        }
    }
    $config.algorithms = $filteredAlgorithms
}

if ($DatasetFilter) {
    Write-Host "Filtering datasets: $DatasetFilter" -ForegroundColor Yellow
    $config.datasets = $config.datasets | Where-Object { $_.Name -like "*$DatasetFilter*" -or $_.Category -like "*$DatasetFilter*" }
}

# Quick test overrides
if ($QuickTest) {
    Write-Host "=== QUICK TEST MODE ===" -ForegroundColor Yellow
    Write-Host "Running minimal test suite for verification..." -ForegroundColor White
    $config.datasets = $config.datasets | Where-Object { $_.Category -eq "P1_Small" }
    foreach ($algName in $config.algorithms.PSObject.Properties.Name) {
        if ($config.algorithms.$algName.parallel) {
            $config.algorithms.$algName.parallel.threadCounts = @(1, 2)
        }
    }
    if ($config.settings) {
        $config.settings.repetitions = 1000
    }
}

# Verify executables exist for enabled algorithms
Write-Host "`nVerifying executables..." -ForegroundColor White
$missingExecutables = @()

foreach ($algorithmName in $config.algorithms.PSObject.Properties.Name) {
    $algorithm = $config.algorithms.$algorithmName
    
    if ($algorithm.sequential -and $algorithm.sequential.enabled) {
        $execPath = $algorithm.sequential.executable
        if (!(Test-Path $execPath)) {
            $missingExecutables += $algorithm.sequential.executable
        }
    }
    
    if ($algorithm.parallel -and $algorithm.parallel.enabled) {
        $execPath = $algorithm.parallel.executable
        if (!(Test-Path $execPath)) {
            $missingExecutables += $algorithm.parallel.executable
        }
    }
}

if ($missingExecutables.Count -gt 0) {
    Write-Host "ERROR: Missing executables:" -ForegroundColor Red
    $missingExecutables | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
    Write-Host "Run build_all.ps1 first to compile all algorithms" -ForegroundColor Red
    exit 1
}

Write-Host "SUCCESS: All required executables found" -ForegroundColor Green

# Clean previous results if enabled
# Output files will be overwritten by each test run.
# No previous results are deleted; only the current test's output is replaced.
# To enable cleaning, uncomment the block below.
<# 
if ($config.settings -and $config.settings.cleanBefore) {
    Write-Host "`n=== CLEANING PREVIOUS RESULTS ===" -ForegroundColor Magenta
    Get-ChildItem -Path "..\Result" -Filter "*.txt" -Recurse -ErrorAction SilentlyContinue | Remove-Item -Force
    Get-ChildItem -Path "..\Logs" -Filter "*.txt" -Recurse -ErrorAction SilentlyContinue | Remove-Item -Force
    Write-Host "Previous results cleaned" -ForegroundColor Green
}
#>

# Create directory structure for all enabled algorithms
Write-Host "`nCreating directory structure..." -ForegroundColor White
foreach ($algorithmName in $config.algorithms.PSObject.Properties.Name) {
    $algorithm = $config.algorithms.$algorithmName
    if (($algorithm.sequential -and $algorithm.sequential.enabled) -or 
        ($algorithm.parallel -and $algorithm.parallel.enabled)) {
        foreach ($dataset in $config.datasets) {
            $resultDir = "..\Result\$algorithmName\$($dataset.Category)"
            $logDir = "..\Logs\$algorithmName\$($dataset.Category)"
            New-Item -ItemType Directory -Path $resultDir -Force | Out-Null
            New-Item -ItemType Directory -Path $logDir -Force | Out-Null
        }
    }
}

# Enhanced makespan extraction with validation
function Get-Makespan {
    param($filePath)
    if (Test-Path $filePath) {
        try {
            $content = Get-Content $filePath
            # Check for different output formats
            foreach ($line in $content) {
                # Format 1: Number on first line (original format)
                if ($line -match '^\d+$') {
                    return [int]$line
                }
                # Format 2: "Best makespan: X" (Branch & Bound format)
                if ($line -match 'Best makespan:\s*(\d+)') {
                    return [int]$matches[1]
                }
                # Format 3: "Makespan: X" (alternative format)
                if ($line -match 'Makespan:\s*(\d+)') {
                    return [int]$matches[1]
                }
            }
        }
        catch {
            Write-Warning "Failed to extract makespan from $filePath"
        }
    }
    return $null
}

# Enhanced operation distribution analysis
function Get-OperationDistribution {
    param($filePath, $threads)
    if (!(Test-Path $filePath)) {
        return @{
            Display         = "No data available"
            JobCount        = 0
            TotalOps        = 0
            AvgOpsPerThread = 0
        }
    }
    
    try {
        $content = Get-Content $filePath
        if ($content.Count -lt 2) {
            return @{
                Display         = "Invalid file format"
                JobCount        = 0
                TotalOps        = 0
                AvgOpsPerThread = 0
            }
        }
        
        # Skip first line (makespan) and count operations per job
        $jobCount = $content.Count - 1
        $totalOperations = 0
        
        for ($i = 1; $i -lt $content.Count; $i++) {
            $operations = ($content[$i] -split " ").Count
            if ($operations -gt 0) {
                $totalOperations += $operations
            }
        }
        
        $avgOpsPerThread = if ($threads -gt 0) { [math]::Round($totalOperations / $threads, 1) } else { 0 }
        
        return @{
            Display         = "Jobs: $jobCount, Total Ops: $totalOperations, Avg Ops/Thread: $avgOpsPerThread"
            JobCount        = $jobCount
            TotalOps        = $totalOperations
            AvgOpsPerThread = $avgOpsPerThread
        }
    }
    catch {
        return @{
            Display         = "Analysis failed"
            JobCount        = 0
            TotalOps        = 0
            AvgOpsPerThread = 0
        }
    }
}

# Performance metrics calculation
function Get-PerformanceMetrics {
    param($results, $dataset)
    
    $metrics = @{
        BestMakespan       = 999999
        BestAlgorithm      = ""
        BestImplementation = ""
        SpeedupData        = @{}
        EfficiencyData     = @{}
    }
    
    $sequentialTime = $null
    
    # Find best makespan and collect speedup data
    foreach ($algName in $results.Keys) {
        if ($results[$algName].ContainsKey("Sequential")) {
            $seqResult = $results[$algName]["Sequential"]
            if ($seqResult.Makespan -lt $metrics.BestMakespan) {
                $metrics.BestMakespan = $seqResult.Makespan
                $metrics.BestAlgorithm = $algName
                $metrics.BestImplementation = "Sequential"
            }
            
            if (-not $sequentialTime -or $seqResult.Time -lt $sequentialTime) {
                $sequentialTime = $seqResult.Time
            }
        }
        
        # Check parallel implementations
        foreach ($implName in $results[$algName].Keys) {
            $result = $results[$algName][$implName]
            if ($result.Makespan -lt $metrics.BestMakespan) {
                $metrics.BestMakespan = $result.Makespan
                $metrics.BestAlgorithm = $algName
                $metrics.BestImplementation = $implName
            }
            
            # Calculate speedup for parallel implementations
            if ($implName -like "Parallel_*" -and $sequentialTime) {
                $speedup = $sequentialTime / $result.Time
                $efficiency = $speedup / $result.ThreadCount * 100
                $metrics.SpeedupData["$algName-$implName"] = [math]::Round($speedup, 2)
                $metrics.EfficiencyData["$algName-$implName"] = [math]::Round($efficiency, 1)
            }
        }
    }
    
    return $metrics
}

# Create result storage for analysis
$results = @{}

# Calculate total tests
$totalTests = 0
foreach ($algorithmName in $config.algorithms.PSObject.Properties.Name) {
    $algorithm = $config.algorithms.$algorithmName
    if ($algorithm.sequential -and $algorithm.sequential.enabled) {
        $totalTests += $config.datasets.Count
    }
    if ($algorithm.parallel -and $algorithm.parallel.enabled) {
        $totalTests += $config.datasets.Count * $algorithm.parallel.threadCounts.Count
    }
}

Write-Host "`n=============================================" -ForegroundColor Magenta
Write-Host "=== RUNNING ALGORITHM TESTS (Total: $totalTests) ===" -ForegroundColor Magenta
Write-Host "=============================================" -ForegroundColor Magenta

$testCount = 0

# Run sequential tests for each enabled algorithm
foreach ($algorithmName in $config.algorithms.PSObject.Properties.Name) {
    $algorithm = $config.algorithms.$algorithmName
    
    if ($algorithm.sequential -and $algorithm.sequential.enabled) {
        Write-Host "`n--- Testing $algorithmName Sequential ---" -ForegroundColor Yellow
        
        foreach ($dataset in $config.datasets) {
            $testCount++
            Write-Host "`n[$testCount/$totalTests] $algorithmName Sequential: $($dataset.Category) ($($dataset.Size))" -ForegroundColor White
            # Verify dataset file exists
            if (!(Test-Path "../Data/$($dataset.Name).jss")) {
                Write-Host "  ERROR: Dataset file not found!" -ForegroundColor Red
                continue
            }
            
            # Create output files
            $resultFile = "..\Result\$algorithmName\$($dataset.Category)\sequential_result.txt"
            $logFile = "..\Logs\$algorithmName\$($dataset.Category)\sequential_log.txt"
            
            # Run algorithm
            Write-Host "  -> Executing $algorithmName sequential algorithm..." -ForegroundColor Gray
            $startTime = Get-Date
            & ".\$($algorithm.sequential.executable)" "../Data/$($dataset.Name).jss" $resultFile 2>$null
            $endTime = Get-Date
            $duration = ($endTime - $startTime).TotalMilliseconds
            
            if ($LASTEXITCODE -eq 0) {
                # Extract makespan
                $makespan = Get-Makespan $resultFile
                
                # Get operation distribution
                $opDistribution = Get-OperationDistribution $resultFile 1
                
                Write-Host "  -> SUCCESS: Completed in $([math]::Round($duration, 0))ms (Makespan: $makespan)" -ForegroundColor Green
                
                # Store results
                if (-not $results.ContainsKey("$algorithmName-$($dataset.Category)")) {
                    $results["$algorithmName-$($dataset.Category)"] = @{}
                }
                $results["$algorithmName-$($dataset.Category)"]["Sequential"] = @{
                    Time           = $duration
                    Makespan       = $makespan
                    OpDistribution = $opDistribution.Display
                    OpData         = $opDistribution
                    ResultFile     = $resultFile
                    Algorithm      = $algorithmName
                    Implementation = "Sequential"
                }
                
                # Create detailed log
                $logContent = @"
=== $algorithmName SEQUENTIAL ALGORITHM RESULTS ===
Dataset: $($dataset.Category) ($($dataset.Size))
Input File: Data/$($dataset.Name).jss
Output File: $resultFile
Execution Time: $([math]::Round($duration, 2))ms
Makespan: $makespan
Operation Distribution: $($opDistribution.Display)
Algorithm: $algorithmName Sequential
Timestamp: $(Get-Date)

=== RESULT ANALYSIS ===
Total scheduling time (makespan): $makespan time units
Algorithm efficiency: Sequential processing
All operations executed in single thread
Jobs processed: $($opDistribution.JobCount)
Total operations: $($opDistribution.TotalOps)
"@
                $logContent | Out-File -FilePath $logFile -Encoding UTF8
            }
            else {
                Write-Host "  -> FAILED: $algorithmName sequential execution failed" -ForegroundColor Red
            }
        }
    }
    
    # Run parallel tests if enabled
    if ($algorithm.parallel -and $algorithm.parallel.enabled) {
        Write-Host "`n--- Testing $algorithmName Parallel ---" -ForegroundColor Yellow
        foreach ($dataset in $config.datasets) {
            # Verify dataset file exists
            if (!(Test-Path "../Data/$($dataset.Name).jss")) {
                Write-Host "  ERROR: Dataset file not found!" -ForegroundColor Red
                continue
            }
            
            foreach ($threads in $algorithm.parallel.threadCounts) {
                $testCount++
                Write-Host "`n[$testCount/$totalTests] $algorithmName Parallel ($threads threads): $($dataset.Category) ($($dataset.Size))" -ForegroundColor White
                
                # Create output files
                $resultFile = "..\Result\$algorithmName\$($dataset.Category)\parallel_${threads}threads_result.txt"
                $logFile = "..\Logs\$algorithmName\$($dataset.Category)\parallel_${threads}threads_log.txt"                # Run parallel algorithm
                Write-Host "  -> Executing $algorithmName parallel algorithm with $threads threads..." -ForegroundColor Gray
                $startTime = Get-Date
                & "$($algorithm.parallel.executable)" "../Data/$($dataset.Name).jss" $resultFile $threads 2>$null
                $endTime = Get-Date
                $duration = ($endTime - $startTime).TotalMilliseconds
                
                if ($LASTEXITCODE -eq 0) {
                    # Extract makespan
                    $makespan = Get-Makespan $resultFile
                    
                    # Get operation distribution
                    $opDistribution = Get-OperationDistribution $resultFile $threads
                    
                    Write-Host "  -> SUCCESS: Completed in $([math]::Round($duration, 0))ms (Makespan: $makespan)" -ForegroundColor Green
                    
                    # Store results
                    if (-not $results.ContainsKey("$algorithmName-$($dataset.Category)")) {
                        $results["$algorithmName-$($dataset.Category)"] = @{}
                    }
                    $results["$algorithmName-$($dataset.Category)"]["Parallel_${threads}threads"] = @{
                        Time           = $duration
                        Makespan       = $makespan
                        OpDistribution = $opDistribution.Display
                        OpData         = $opDistribution
                        ResultFile     = $resultFile
                        ThreadCount    = $threads
                        Algorithm      = $algorithmName
                        Implementation = "Parallel_${threads}threads"
                    }
                    
                    # Create detailed log
                    $logContent = @"
=== $algorithmName PARALLEL ALGORITHM RESULTS ===
Dataset: $($dataset.Category) ($($dataset.Size))
Input File: Data/$($dataset.Name).jss
Output File: $resultFile
Execution Time: $([math]::Round($duration, 2))ms
Thread Count: $threads
Makespan: $makespan
Operation Distribution: $($opDistribution.Display)
Algorithm: $algorithmName Parallel
Timestamp: $(Get-Date)

=== RESULT ANALYSIS ===
Total scheduling time (makespan): $makespan time units
Thread utilization: $threads threads
Jobs processed: $($opDistribution.JobCount)
Total operations: $($opDistribution.TotalOps)
Average operations per thread: $($opDistribution.AvgOpsPerThread)
"@
                    $logContent | Out-File -FilePath $logFile -Encoding UTF8
                }
                else {
                    Write-Host "  -> FAILED: $algorithmName parallel execution failed" -ForegroundColor Red
                }
            }
        }
    }
}

# Generate comprehensive analysis report
Write-Host "`n=============================================" -ForegroundColor Cyan
Write-Host "=== GENERATING ANALYSIS REPORT ===" -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan

$reportFile = "ultimate_analysis_report.txt"
$reportContent = @"
========================================================================
=== ULTIMATE JOB SHOP SCHEDULING ANALYSIS REPORT ===
========================================================================
Generated: $(Get-Date)
Configuration: $ConfigFile
Total Tests Executed: $testCount

=== ALGORITHM PERFORMANCE SUMMARY ===

"@

# Group results by dataset for comparison
$datasetGroups = @{}
foreach ($key in $results.Keys) {
    $parts = $key -split '-'
    $algorithm = $parts[0]
    $dataset = $parts[1]
    
    if (-not $datasetGroups.ContainsKey($dataset)) {
        $datasetGroups[$dataset] = @{}
    }
    $datasetGroups[$dataset][$algorithm] = $results[$key]
}

foreach ($dataset in $datasetGroups.Keys | Sort-Object) {
    $reportContent += "`n--- $dataset Results ---`n"
    $reportContent += "Algorithm".PadRight(20) + "Implementation".PadRight(20) + "Makespan".PadRight(12) + "Time(ms)".PadRight(12) + "Operation Distribution`n"
    $reportContent += ("-" * 100) + "`n"
    
    foreach ($algorithm in $datasetGroups[$dataset].Keys | Sort-Object) {
        $algorithmResults = $datasetGroups[$dataset][$algorithm]
        
        foreach ($implementation in $algorithmResults.Keys | Sort-Object) {
            $result = $algorithmResults[$implementation]
            $makespan = if ($result.Makespan) { $result.Makespan.ToString() } else { "N/A" }
            $time = [math]::Round($result.Time, 1).ToString()
            
            $reportContent += $algorithm.PadRight(20) + $implementation.PadRight(20) + $makespan.PadRight(12) + $time.PadRight(12) + $result.OpDistribution + "`n"
        }
    }
}

$reportContent += @"

=== PERFORMANCE INSIGHTS ===

This report compares the performance of different scheduling algorithms:
* Makespan: Lower values indicate better scheduling efficiency
* Execution Time: Algorithm computational performance
* Operation Distribution: How work is divided among threads (for parallel implementations)

=== ALGORITHM DESCRIPTIONS ===
* Greedy: Earliest start time heuristic
* SPT: Shortest Processing Time first
* LPT: Longest Processing Time first  
* MOR: Most Operations Remaining priority
* ShiftingBottleneck: Shifting Bottleneck heuristic

=== FILES GENERATED ===
* Results: Result/[Algorithm]/[Category]/
* Logs: Logs/[Algorithm]/[Category]/
* Configuration: $ConfigFile

=== USAGE EXAMPLES ===
.\ultimate_analysis.ps1                    # Run all enabled algorithms
.\ultimate_analysis.ps1 -QuickTest         # Quick test with small dataset only
.\ultimate_analysis.ps1 -AlgorithmFilter SPT # Run only SPT algorithm
.\ultimate_analysis.ps1 -DatasetFilter Small # Run only small dataset
.\ultimate_analysis.ps1 -CleanOnly         # Clean all previous results
.\ultimate_analysis.ps1 -GenerateConfig    # Generate fresh config file

========================================================================
"@

$reportContent | Out-File -FilePath $reportFile -Encoding UTF8

Write-Host "SUCCESS: Ultimate analysis completed!" -ForegroundColor Green
Write-Host "Report saved to: $reportFile" -ForegroundColor White
Write-Host "Total tests executed: $testCount" -ForegroundColor White

# Display summary table
Write-Host "`n=== PERFORMANCE SUMMARY ===" -ForegroundColor Cyan
Write-Host "Algorithm".PadRight(15) + "Dataset".PadRight(15) + "Best Makespan".PadRight(15) + "Best Implementation" -ForegroundColor White
Write-Host ("-" * 65) -ForegroundColor Gray

foreach ($dataset in $datasetGroups.Keys | Sort-Object) {
    $bestMakespan = 999999
    $bestImpl = ""
    $bestAlgorithm = ""
    
    foreach ($algorithm in $datasetGroups[$dataset].Keys) {
        $algorithmResults = $datasetGroups[$dataset][$algorithm]
        
        foreach ($implementation in $algorithmResults.Keys) {
            $result = $algorithmResults[$implementation]
            if ($result.Makespan -and $result.Makespan -lt $bestMakespan) {
                $bestMakespan = $result.Makespan
                $bestImpl = $implementation
                $bestAlgorithm = $algorithm
            }
        }
    }
    
    if ($bestMakespan -lt 999999) {
        Write-Host $bestAlgorithm.PadRight(15) + $dataset.PadRight(15) + $bestMakespan.ToString().PadRight(15) + $bestImpl -ForegroundColor Green
    }
}

Write-Host "`n=== NEXT STEPS ===" -ForegroundColor Yellow
Write-Host "* To reconfigure algorithms and thread counts, edit: $ConfigFile" -ForegroundColor White
Write-Host "* To test specific algorithms: .\ultimate_analysis.ps1 -AlgorithmFilter <name>" -ForegroundColor White
Write-Host "* To test specific datasets: .\ultimate_analysis.ps1 -DatasetFilter <name>" -ForegroundColor White
Write-Host "* For quick verification: .\ultimate_analysis.ps1 -QuickTest" -ForegroundColor White
