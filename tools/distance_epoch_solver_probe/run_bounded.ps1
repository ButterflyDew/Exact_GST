[CmdletBinding()]
param(
    [string]$DataRoot = "data",
    [string]$GraphSelector = "DBLP",
    [string]$QuerySelector = "g13",
    [int]$QueryIndex = 1,
    [ValidateSet("--distance", "--hybrid", "--delayed-upper", "--delayed-upper-verbose", "--early-delayed", "--early-delayed-verbose")]
    [string]$Mode = "--distance",
    [ValidateRange(1, 86400)]
    [int]$Seconds = 900
)

$ErrorActionPreference = "Stop"
$workspace = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$exe = Join-Path $workspace "build\tools\distance_epoch_solver_probe\Release\gst_distance_epoch_solver_probe.exe"
if (-not (Test-Path -LiteralPath $exe)) {
    throw "Missing executable: $exe"
}

function Quote-ProcessArgument([string]$Value) {
    return '"' + ($Value -replace '(\*)"', '$1$1\"' -replace '(\+)$', '$1$1') + '"'
}

$arguments = @(
    $Mode,
    $DataRoot,
    $GraphSelector,
    $QuerySelector,
    $QueryIndex.ToString(),
    "1"
)
$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $exe
$startInfo.Arguments = (($arguments | ForEach-Object { Quote-ProcessArgument $_ }) -join ' ')
$startInfo.WorkingDirectory = $workspace
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true

$process = New-Object System.Diagnostics.Process
$process.StartInfo = $startInfo
try {
    if (-not $process.Start()) {
        throw "Failed to start $exe"
    }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    [long]$sampledPeak = 0
    while (-not $process.HasExited -and $watch.Elapsed.TotalSeconds -lt $Seconds) {
        $process.Refresh()
        $sampledPeak = [Math]::Max($sampledPeak, $process.WorkingSet64)
        Start-Sleep -Milliseconds 250
    }
    $completed = $process.HasExited
    if (-not $completed) {
        $process.Kill()
        $process.WaitForExit()
        Write-Output "bounded_timeout=1 pid=$($process.Id) seconds=$Seconds sampled_peak_mb=$([Math]::Round($sampledPeak / 1MB, 3)) mode=$Mode"
    }
    else {
        Write-Output "bounded_timeout=0 pid=$($process.Id) exit=$($process.ExitCode) sampled_peak_mb=$([Math]::Round($sampledPeak / 1MB, 3)) mode=$Mode"
    }
    if ($stderrTask.Result) {
        Write-Output $stderrTask.Result.TrimEnd()
    }
    if ($stdoutTask.Result) {
        Write-Output $stdoutTask.Result.TrimEnd()
    }
    if (-not $completed) {
        exit 124
    }
    exit $process.ExitCode
}
finally {
    if (-not $process.HasExited) {
        $process.Kill()
        $process.WaitForExit()
    }
    $process.Dispose()
}
