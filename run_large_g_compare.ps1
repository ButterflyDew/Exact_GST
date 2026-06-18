param(
    [string]$Config = "Release",
    [string]$DataRoot = "data_new",
    [string]$Graph = "DBLP",
    [string[]]$Queries = @(
        "g10_uniform",
        "g10_nonuniform"
    ),
    [string[]]$Methods = @(
        "PrunedDP",
        "Test11"
    ),
    [int]$QueryBegin = 1,
    [int]$QueryLimit = 10,
    [string]$ResultRoot = "result_large_g_compare",
    [string]$DebugRoot = "debug_large_g_compare",
    [string]$RootPolicy = "child_first",
    [switch]$Build,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$methodExe = @{
    "PrunedDP" = "gst_pruned_dp_main.exe"
    "Test8"    = "gst_test8_main.exe"
    "Test10"   = "gst_test10_main.exe"
    "Test11"   = "gst_test11_main.exe"
    "Test12"   = "gst_test12_main.exe"
}

function Resolve-ExePath {
    param([string]$Method)

    if (-not $methodExe.ContainsKey($Method)) {
        throw "Unknown method '$Method'. Known methods: $($methodExe.Keys -join ', ')"
    }

    return Join-Path -Path $PSScriptRoot -ChildPath "build\$Config\$($methodExe[$Method])"
}

function Invoke-Or-Print {
    param(
        [string]$Label,
        [string]$ExePath,
        [string[]]$Arguments
    )

    $cmdText = "& `"$ExePath`" $($Arguments -join ' ')"
    Write-Host ""
    Write-Host "==== $Label ===="
    Write-Host $cmdText

    if ($DryRun) {
        return
    }

    $start = Get-Date
    & $ExePath @Arguments
    $exitCode = $LASTEXITCODE
    $elapsed = (Get-Date) - $start
    Write-Host "==== done: $Label exit=$exitCode elapsed=$($elapsed.ToString()) ===="

    if ($exitCode -ne 0) {
        throw "Command failed: $Label"
    }
}

Set-Location $PSScriptRoot

if ($Build) {
    foreach ($method in $Methods) {
        $exeName = $methodExe[$method]
        if (-not $exeName) {
            throw "Unknown method '$method'. Known methods: $($methodExe.Keys -join ', ')"
        }

        $target = [System.IO.Path]::GetFileNameWithoutExtension($exeName)
        $buildArgs = @("--build", "build", "--config", $Config, "--target", $target)
        Write-Host ""
        Write-Host "==== build $target ($Config) ===="
        Write-Host "cmake $($buildArgs -join ' ')"

        if (-not $DryRun) {
            & cmake @buildArgs
            if ($LASTEXITCODE -ne 0) {
                throw "Build failed: $target"
            }
        }
    }
}

$runStarted = Get-Date -Format "yyyyMMdd_HHmmss"
$logDir = Join-Path $PSScriptRoot "run_logs"
if (-not $DryRun -and -not (Test-Path $logDir)) {
    New-Item -ItemType Directory -Path $logDir | Out-Null
}
$logPath = Join-Path $logDir "large_g_compare_$runStarted.log"

Write-Host "Config=$Config"
Write-Host "DataRoot=$DataRoot Graph=$Graph"
Write-Host "Queries=$($Queries -join ',')"
Write-Host "Methods=$($Methods -join ',')"
Write-Host "QueryBegin=$QueryBegin QueryLimit=$QueryLimit"
Write-Host "ResultRoot=$ResultRoot DebugRoot=$DebugRoot"
if (-not $DryRun) {
    "Run started $(Get-Date)" | Out-File -FilePath $logPath -Encoding utf8
}

foreach ($query in $Queries) {
    foreach ($method in $Methods) {
        $exePath = Resolve-ExePath $method
        if (-not (Test-Path $exePath)) {
            throw "Executable not found: $exePath. Re-run with -Build or build it manually."
        }

        $label = "$method $Graph $query begin=$QueryBegin limit=$QueryLimit data=$DataRoot"
        $args = @(
            $Graph,
            "weight",
            $ResultRoot,
            $DebugRoot,
            $RootPolicy,
            $query,
            $DataRoot,
            [string]$QueryBegin,
            [string]$QueryLimit
        )

        if ($DryRun) {
            Invoke-Or-Print -Label $label -ExePath $exePath -Arguments $args
        }
        else {
            try {
                "==== $label ====" | Tee-Object -FilePath $logPath -Append
                Invoke-Or-Print -Label $label -ExePath $exePath -Arguments $args 2>&1 |
                    Tee-Object -FilePath $logPath -Append
            }
            catch {
                "FAILED: $label" | Tee-Object -FilePath $logPath -Append
                throw
            }
        }
    }
}

Write-Host ""
Write-Host "All runs finished."
if (-not $DryRun) {
    Write-Host "Log: $logPath"
    Write-Host "Results under: $ResultRoot\weight\$Graph\<Method>\<query_selector>\"
}
