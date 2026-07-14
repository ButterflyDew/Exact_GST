param(
    [string]$Executable = ".\build\Release\gst_test80_main.exe",
    [datetime]$Deadline = "2026-07-14 11:20:00",
    [int]$FirstQuery = 2,
    [int]$LastQuery = 40
)

$ErrorActionPreference = "Stop"
$exe = (Resolve-Path -LiteralPath $Executable).Path
$statsPath = "result\DBLP\Test80\query_g13\test80_stats.txt"
$logPath = "result\DBLP\Test80\query_g13\cross_query_runner.log"
$logDir = Split-Path -Parent $logPath
New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$completed = [System.Collections.Generic.HashSet[int]]::new()
if (Test-Path -LiteralPath $statsPath) {
    foreach ($line in Get-Content -LiteralPath $statsPath -Encoding UTF8) {
        if ($line -match '^query=(\d+) .* root_star_upper=') {
            [void]$completed.Add([int]$matches[1])
        }
    }
}

# query_g13 was generated in four ten-query group-size bands. Interleaving the
# bands preserves structural coverage if the user-specified deadline arrives
# before every query finishes. Query 1 is excluded because its full run exists.
$order = [System.Collections.Generic.List[int]]::new()
for ($offset = 0; $offset -lt 10; ++$offset) {
    $queries = @(($offset + 11), ($offset + 21), ($offset + 31))
    if ($offset + 2 -le 10) {
        $queries = @(($offset + 2)) + $queries
    }
    foreach ($query in $queries) {
        if ($query -ge $FirstQuery -and $query -le $LastQuery -and $query -le 40) {
            [void]$order.Add($query)
        }
    }
}

$started = Get-Date
Add-Content -LiteralPath $logPath -Encoding UTF8 -Value (
    "run_start={0:yyyy-MM-dd HH:mm:ss} deadline={1:yyyy-MM-dd HH:mm:ss} exe={2}" -f `
        $started, $Deadline, $exe)

foreach ($query in $order) {
    if ($completed.Contains($query)) {
        continue
    }
    if ((Get-Date) -ge $Deadline) {
        Add-Content -LiteralPath $logPath -Encoding UTF8 -Value (
            "deadline_reached={0:yyyy-MM-dd HH:mm:ss} next_query={1}" -f (Get-Date), $query)
        break
    }

    $queryStart = Get-Date
    Add-Content -LiteralPath $logPath -Encoding UTF8 -Value (
        "query_start={0:yyyy-MM-dd HH:mm:ss} query={1}" -f $queryStart, $query)
    $output = & $exe DBLP result g13 data $query 1 2>&1
    $exitCode = $LASTEXITCODE
    $queryEnd = Get-Date
    $output | ForEach-Object { Add-Content -LiteralPath $logPath -Encoding UTF8 -Value $_ }
    Add-Content -LiteralPath $logPath -Encoding UTF8 -Value (
        "query_end={0:yyyy-MM-dd HH:mm:ss} query={1} elapsed_s={2:F3} exit={3}" -f `
            $queryEnd, $query, ($queryEnd - $queryStart).TotalSeconds, $exitCode)
    $output | Write-Output
    if ($exitCode -ne 0) {
        throw "Test80 failed for DBLP g13 query $query with exit code $exitCode."
    }
    [void]$completed.Add($query)
}

$finished = Get-Date
Add-Content -LiteralPath $logPath -Encoding UTF8 -Value (
    "run_end={0:yyyy-MM-dd HH:mm:ss} elapsed_s={1:F3}" -f `
        $finished, ($finished - $started).TotalSeconds)
