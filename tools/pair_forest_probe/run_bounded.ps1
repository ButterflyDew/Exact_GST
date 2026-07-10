[CmdletBinding()]
param(
    [string]$DataRoot = "data",
    [string]$GraphSelector = "DBLP",
    [string]$QuerySelector = "g13",
    [double]$KnownOptimum = 12.5936282853,
    [int]$QueryIndex = 1,
    [ValidateRange(1, 86400)]
    [int]$Seconds = 900
)

$ErrorActionPreference = "Stop"
$workspace = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$exe = Join-Path $workspace "build\tools\pair_forest_probe\Release\gst_pair_forest_probe.exe"
if (-not (Test-Path -LiteralPath $exe)) {
    throw "Missing executable: $exe"
}

function Quote-ProcessArgument([string]$Value) {
    return '"' + ($Value -replace '(\*)"', '$1$1\"' -replace '(\+)$', '$1$1') + '"'
}

$arguments = @(
    $DataRoot,
    $GraphSelector,
    $QuerySelector,
    $KnownOptimum.ToString([Globalization.CultureInfo]::InvariantCulture),
    $QueryIndex.ToString()
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
    $completed = $process.WaitForExit($Seconds * 1000)
    if (-not $completed) {
        $process.Kill()
        $process.WaitForExit()
        Write-Output "bounded_timeout=1 pid=$($process.Id) seconds=$Seconds"
    }
    else {
        Write-Output "bounded_timeout=0 pid=$($process.Id) exit=$($process.ExitCode)"
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

