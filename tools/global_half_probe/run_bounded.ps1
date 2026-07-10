[CmdletBinding()]
param(
    [string]$GraphFolder = "data\DBLP",
    [string]$QuerySelector = "g13",
    [int]$QueryIndex = 1,
    [int]$AnchorGroup = 1,
    [ValidateRange(1, 86400)]
    [int]$Seconds = 120
)

$ErrorActionPreference = "Stop"
$workspace = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$exe = Join-Path $workspace "build\tools\global_half_probe\Release\gst_global_half_probe.exe"
if (-not (Test-Path -LiteralPath $exe)) {
    throw "Missing executable: $exe"
}

function Quote-ProcessArgument([string]$Value) {
    return '"' + ($Value -replace '(\\*)"', '$1$1\"' -replace '(\\+)$', '$1$1') + '"'
}

$arguments = @(
    "--dual-anchored-dataset",
    $GraphFolder,
    $QuerySelector,
    $QueryIndex.ToString(),
    $AnchorGroup.ToString()
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
$oldProgress = $env:GST_GLOBAL_LABEL_PROGRESS

try {
    $env:GST_GLOBAL_LABEL_PROGRESS = "1"
    if (-not $process.Start()) {
        throw "Failed to start $exe"
    }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $peakWorkingSet = 0L
    $completed = $false
    while ($timer.Elapsed.TotalSeconds -lt $Seconds) {
        $process.Refresh()
        $peakWorkingSet = [Math]::Max($peakWorkingSet, $process.WorkingSet64)
        if ($process.WaitForExit(250)) {
            $completed = $true
            break
        }
    }
    if (-not $completed) {
        $process.Kill()
        $process.WaitForExit()
        Write-Output "bounded_timeout=1 pid=$($process.Id) seconds=$Seconds"
    }
    else {
        Write-Output "bounded_timeout=0 pid=$($process.Id) exit=$($process.ExitCode)"
    }
    $process.Refresh()
    $peakWorkingSet = [Math]::Max($peakWorkingSet, $process.PeakWorkingSet64)
    Write-Output ("bounded_elapsed_sec={0:F3} bounded_peak_rss_mb={1:F3}" -f `
        $timer.Elapsed.TotalSeconds, ($peakWorkingSet / 1MB))

    $stderr = $stderrTask.Result
    $stdout = $stdoutTask.Result
    if ($stderr) {
        Write-Output $stderr.TrimEnd()
    }
    if ($stdout) {
        Write-Output $stdout.TrimEnd()
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
    $env:GST_GLOBAL_LABEL_PROGRESS = $oldProgress
    $process.Dispose()
}
