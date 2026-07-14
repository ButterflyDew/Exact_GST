param(
    [string]$StatsPath = "result\DBLP\Test80\query_g13\test80_stats.txt",
    [string]$OutputDirectory = "result\DBLP\Test80\query_g13"
)

$ErrorActionPreference = "Stop"
$invariant = [Globalization.CultureInfo]::InvariantCulture

function Parse-StatsLine([string]$line) {
    $values = @{}
    foreach ($token in $line -split ' ') {
        if ($token -match '^([^=]+)=(.*)$') {
            $values[$matches[1]] = $matches[2]
        }
    }
    return $values
}

function Long($values, [string]$name) {
    if (-not $values.ContainsKey($name)) { return 0L }
    return [long]::Parse($values[$name], $invariant)
}

function Double($values, [string]$name) {
    if (-not $values.ContainsKey($name)) { return 0.0 }
    return [double]::Parse($values[$name], $invariant)
}

function Sum-Sizes($values, [string]$prefix, [int]$first, [int]$last) {
    $sum = 0L
    for ($size = $first; $size -le $last; ++$size) {
        $sum += Long $values ($prefix + $size)
    }
    return $sum
}

$latest = @{}
foreach ($line in Get-Content -LiteralPath $StatsPath -Encoding UTF8) {
    if ($line -notmatch '^query=(\d+) .* root_star_upper=') { continue }
    $latest[[int]$matches[1]] = Parse-StatsLine $line
}

$summary = [System.Collections.Generic.List[object]]::new()
$layers = [System.Collections.Generic.List[object]]::new()
foreach ($query in ($latest.Keys | Sort-Object)) {
    $v = $latest[$query]
    $half = [int][math]::Floor((Long $v "g") / 2)
    $dRowBytes = Sum-Sizes $v "d_row_bytes_s" 1 $half
    $aRowBytes = Sum-Sizes $v "a_row_bytes_s" 0 ($half - 1)
    $dH = Sum-Sizes $v "d_h_evals_s" 1 $half
    $aH = Sum-Sizes $v "a_h_evals_s" 0 ($half - 1)
    $dBound = (Sum-Sizes $v "d_seed_bound_s" 1 $half) +
              (Sum-Sizes $v "d_relax_bound_s" 1 $half)
    $aBound = (Sum-Sizes $v "a_seed_bound_s" 0 ($half - 1)) +
              (Sum-Sizes $v "a_relax_bound_s" 0 ($half - 1))

    $summary.Add([pscustomobject][ordered]@{
        query = $query
        best = Double $v "best"
        wall_ms = Double $v "wall_ms"
        peak_rss_mb = Double $v "peak_rss_mb"
        group_vertices = Long $v "group_vertices"
        min_group_size = Long $v "min_group_size"
        max_group_size = Long $v "max_group_size"
        anchor_group = Long $v "anchor_group"
        anchor_group_size = Long $v "anchor_group_size"
        root_star_upper = Double $v "root_star_upper"
        dual_primal_upper = Double $v "dual_primal_upper"
        junction_before = Double $v "junction_before"
        junction_upper = Double $v "junction_upper"
        junction_after = Double $v "junction_after"
        root_tour_lower = Double $v "root_tour_lower"
        root_dual_lower = Double $v "root_dual_lower"
        root_group_distance_max = Double $v "root_group_dist_max"
        root_group_distance_second_max = Double $v "root_group_dist_second_max"
        junction_path_vertices = Long $v "junction_path_vertices"
        junction_tree_vertices = Long $v "junction_tree_vertices"
        junction_work = Long $v "junction_work"
        packing_trigger_pair_rows = Long $v "packing_trigger_pair_rows"
        packing_trigger_pair_work = Long $v "packing_trigger_pair_work"
        packing_trigger_pair_values = Long $v "packing_trigger_pair_values"
        packing_rounds = Long $v "packing_rounds"
        packing_scale_average = Double $v "packing_scale_avg"
        ordinary_values = Long $v "ordinary_values"
        anchored_values = Long $v "anchored_values"
        ordinary_pops = Long $v "ordinary_pops"
        anchored_pops = Long $v "anchored_pops"
        completion_checks = Long $v "completion_checks"
        group_distance_bytes = Long $v "group_dist_bytes"
        ordinary_row_bytes = $dRowBytes
        anchored_row_bytes = $aRowBytes
        ordinary_h_evals = $dH
        anchored_h_evals = $aH
        ordinary_h_farthest = Sum-Sizes $v "d_h_far_s" 1 $half
        ordinary_h_tour = Sum-Sizes $v "d_h_tour_s" 1 $half
        ordinary_h_dual = Sum-Sizes $v "d_h_dual_s" 1 $half
        anchored_h_farthest = Sum-Sizes $v "a_h_far_s" 0 ($half - 1)
        anchored_h_tour = Sum-Sizes $v "a_h_tour_s" 0 ($half - 1)
        anchored_h_dual = Sum-Sizes $v "a_h_dual_s" 0 ($half - 1)
        ordinary_merge_candidates = Sum-Sizes $v "d_seed_candidates_s" 1 $half
        ordinary_merge_old_rejects = Sum-Sizes $v "d_seed_old_s" 1 $half
        ordinary_merge_bound_rejects = Sum-Sizes $v "d_seed_bound_s" 1 $half
        ordinary_relax_attempts = Sum-Sizes $v "d_relax_s" 1 $half
        ordinary_relax_old_rejects = Sum-Sizes $v "d_relax_old_s" 1 $half
        ordinary_relax_bound_rejects = Sum-Sizes $v "d_relax_bound_s" 1 $half
        anchored_merge_candidates = Sum-Sizes $v "a_seed_candidates_s" 0 ($half - 1)
        anchored_merge_old_rejects = Sum-Sizes $v "a_seed_old_s" 0 ($half - 1)
        anchored_merge_bound_rejects = Sum-Sizes $v "a_seed_bound_s" 0 ($half - 1)
        anchored_relax_attempts = Sum-Sizes $v "a_relax_s" 0 ($half - 1)
        anchored_relax_old_rejects = Sum-Sizes $v "a_relax_old_s" 0 ($half - 1)
        anchored_relax_bound_rejects = Sum-Sizes $v "a_relax_bound_s" 0 ($half - 1)
        ordinary_bound_rejects = $dBound
        anchored_bound_rejects = $aBound
        ordinary_join_direct_calls = Long $v "d_join_direct_calls"
        ordinary_join_direct_work = Long $v "d_join_direct_work"
        ordinary_join_binary_calls = Long $v "d_join_binary_calls"
        ordinary_join_binary_work = Long $v "d_join_binary_work"
        ordinary_join_linear_calls = Long $v "d_join_linear_calls"
        ordinary_join_linear_work = Long $v "d_join_linear_work"
        ordinary_join_work = (Long $v "d_join_direct_work") +
                             (Long $v "d_join_binary_work") +
                             (Long $v "d_join_linear_work")
        anchored_join_direct_calls = Long $v "a_join_direct_calls"
        anchored_join_direct_work = Long $v "a_join_direct_work"
        anchored_join_binary_calls = Long $v "a_join_binary_calls"
        anchored_join_binary_work = Long $v "a_join_binary_work"
        anchored_join_linear_calls = Long $v "a_join_linear_calls"
        anchored_join_linear_work = Long $v "a_join_linear_work"
        anchored_join_work = (Long $v "a_join_direct_work") +
                             (Long $v "a_join_binary_work") +
                             (Long $v "a_join_linear_work")
        completion_scan_vertices = Long $v "completion_scan_vertices"
        group_dist_ms = Double $v "group_dist_ms"
        dual_ms = Double $v "dual_ms"
        junction_ms = Double $v "junction_ms"
        packing_ms = Double $v "packing_ms"
        ordinary_ms = Double $v "ordinary_ms"
        anchored_ms = Double $v "anchored_ms"
        completion_ms = Double $v "completion_ms"
    })

    for ($size = 1; $size -le $half; ++$size) {
        $layers.Add([pscustomobject][ordered]@{
            query = $query; phase = "D"; size = $size
            masks = Long $v "d_masks_s$size"
            values = Long $v "d_values_s$size"
            branches = Long $v "d_branches_s$size"
            touched = Long $v "d_seeds_s$size"
            pops = Long $v "d_pops_s$size"
            dense_rows = Long $v "d_dense_rows_s$size"
            sparse_rows = Long $v "d_sparse_rows_s$size"
            row_bytes = Long $v "d_row_bytes_s$size"
            merge_candidates = Long $v "d_seed_candidates_s$size"
            merge_old_rejects = Long $v "d_seed_old_s$size"
            merge_bound_rejects = Long $v "d_seed_bound_s$size"
            relax_attempts = Long $v "d_relax_s$size"
            relax_old_rejects = Long $v "d_relax_old_s$size"
            relax_bound_rejects = Long $v "d_relax_bound_s$size"
            h_evals = Long $v "d_h_evals_s$size"
            h_farthest = Long $v "d_h_far_s$size"
            h_tour = Long $v "d_h_tour_s$size"
            h_dual = Long $v "d_h_dual_s$size"
            completion_scans = 0L
            completion_checks = 0L
            milliseconds = Double $v "d_ms_s$size"
            best_after = Double $v "best_after_d_s$size"
        })
    }
    for ($size = 0; $size -lt $half; ++$size) {
        $layers.Add([pscustomobject][ordered]@{
            query = $query; phase = "A"; size = $size
            masks = Long $v "a_masks_s$size"
            values = Long $v "a_values_s$size"
            branches = 0L
            touched = Long $v "a_touched_s$size"
            pops = Long $v "a_pops_s$size"
            dense_rows = Long $v "a_dense_rows_s$size"
            sparse_rows = Long $v "a_sparse_rows_s$size"
            row_bytes = Long $v "a_row_bytes_s$size"
            merge_candidates = Long $v "a_seed_candidates_s$size"
            merge_old_rejects = Long $v "a_seed_old_s$size"
            merge_bound_rejects = Long $v "a_seed_bound_s$size"
            relax_attempts = Long $v "a_relax_s$size"
            relax_old_rejects = Long $v "a_relax_old_s$size"
            relax_bound_rejects = Long $v "a_relax_bound_s$size"
            h_evals = Long $v "a_h_evals_s$size"
            h_farthest = Long $v "a_h_far_s$size"
            h_tour = Long $v "a_h_tour_s$size"
            h_dual = Long $v "a_h_dual_s$size"
            completion_scans = Long $v "completion_scan_s$size"
            completion_checks = Long $v "completion_checks_s$size"
            milliseconds = Double $v "a_ms_s$size"
            best_after = Double $v "best_after_a_s$size"
        })
    }
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$summaryPath = Join-Path $OutputDirectory "test80_cross_query_summary.csv"
$layerPath = Join-Path $OutputDirectory "test80_cross_query_layers.csv"
$summary | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8
$layers | Export-Csv -LiteralPath $layerPath -NoTypeInformation -Encoding UTF8
Write-Output "queries=$($summary.Count) summary=$summaryPath layers=$layerPath"
