param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [switch]$Check,
    [switch]$DebugBuild
)
$ErrorActionPreference = 'Stop'
$householdRoot = Split-Path -Parent $PSScriptRoot
$householdOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $householdOutput) { throw 'Choose a fresh measurement directory.' }
if ($DebugBuild -and !$Check) { throw 'Performance samples require the separate Release build.' }
$householdBuild = if ($DebugBuild) { 'debug' } else { 'p10-release' }
$householdExecutable = Join-Path $householdRoot ('build/' + $householdBuild + '/bin/household_measure.exe')
if (!(Test-Path -LiteralPath $householdExecutable)) { throw 'Build household_measure in the selected tree first.' }

# This runner uses a foreground fullscreen window. Run it only in the isolated
# measurement desktop or after authorization for this desktop run. It neither
# builds targets nor stops unrelated applications.
function Assert-HouseholdMeasurementIdle {
    $householdConflicts = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $_.ProcessName -match '^(cmake|ninja|ctest|clang.*|cl|link|lld.*|engine_tests|near_laugh|level_editor|household_measure|interior_lighting_measure|scripted_character_measure|character_animation_viewer|character_animation_smoke|.*_smoke|.*_measure)$'
    } | Select-Object ProcessName, Id)
    if ($householdConflicts.Count -ne 0) {
        $householdConflictText = ($householdConflicts | ForEach-Object { $_.ProcessName + '(' + $_.Id + ')' }) -join ', '
        throw ('Concurrent build/test/game/GPU workload detected: ' + $householdConflictText)
    }
}
Assert-HouseholdMeasurementIdle

$householdProfiles = @(
    @{ Name = 'baseline'; Level = (Join-Path $householdRoot 'resources/levels/household-baseline.level.json') },
    @{ Name = 'capacity'; Level = (Join-Path $householdRoot 'resources/levels/household-capacity.level.json') }
)
$householdBaseline = Get-Content -Raw -Encoding UTF8 -LiteralPath $householdProfiles[0].Level | ConvertFrom-Json
$householdCapacity = Get-Content -Raw -Encoding UTF8 -LiteralPath $householdProfiles[1].Level | ConvertFrom-Json
if (@($householdBaseline.household.boxes).Count -ne 0 -or @($householdCapacity.household.boxes).Count -ne 16) {
    throw 'Prepared measurement profiles must contain zero and sixteen boxes.'
}
$householdCapacity.household.boxes = @()
if (($householdBaseline | ConvertTo-Json -Depth 100 -Compress) -cne ($householdCapacity | ConvertTo-Json -Depth 100 -Compress)) {
    throw 'Baseline and capacity must have identical furnishing, documents, radio, routes, doors and lights.'
}
[void](New-Item -ItemType Directory -Path $householdOutput)
$householdUtf8 = New-Object System.Text.UTF8Encoding($false)
$householdMetadata = [ordered]@{
    started_utc = [DateTime]::UtcNow.ToString('o')
    mode = $(if ($Check) { 'check-not-performance' } else { 'measure' })
    build = $householdBuild
    executable = $householdExecutable
    executable_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $householdExecutable).Hash
    resolution = '1920x1080'
    refresh_hz = 60
    present_mode = 'FIFO'
    warmup_seconds = 10
    capture_seconds = 60
    repeats_per_workload = $(if ($Check) { 1 } else { 3 })
    audio_output = 'silent'
    physics_profile = '0.30m cube;1kg;gravity=-18;linear/angular caps=12;LinearCast;2 substeps;80N/4Nm hold;5Hz critical damping;throw=6Ns'
    workload = '16 actual post-step awake bodies;2Ns vertical pulses/15 steps;120-step cycles;60-step hold;alternating drop/throw;authored walker and door cycles'
    process_check_scope = 'Known compiler/build/test/game/measurement processes; other GPU applications must be idle in the authorized test environment'
    cpu = @(Get-CimInstance Win32_Processor | Select-Object Name, NumberOfCores, NumberOfLogicalProcessors, MaxClockSpeed)
    gpu = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion, AdapterRAM, CurrentHorizontalResolution, CurrentVerticalResolution, CurrentRefreshRate)
    os = @(Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber)
    levels = @($householdProfiles | ForEach-Object {
        [ordered]@{ workload = $_.Name; path = $_.Level; sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.Level).Hash }
    })
}
[System.IO.File]::WriteAllText((Join-Path $householdOutput 'configuration.json'), ($householdMetadata | ConvertTo-Json -Depth 12), $householdUtf8)

if (!('HouseholdMeasureDesktop' -as [type])) { Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class HouseholdMeasureDesktop {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int command);
}
'@
}
$householdRepeats = if ($Check) { 1 } else { 3 }
$householdMode = if ($Check) { '--check' } else { '--measure' }
$householdCsvFiles = @()
$householdRuns = @()
for ($householdRepeat = 1; $householdRepeat -le $householdRepeats; ++$householdRepeat) {
    # Alternate paired order to expose sustained-machine drift in raw results.
    $householdOrder = if ($householdRepeat % 2 -eq 1) { @(0,1) } else { @(1,0) }
    foreach ($householdIndex in $householdOrder) {
        Assert-HouseholdMeasurementIdle
        $householdProfile = $householdProfiles[$householdIndex]
        $householdName = $householdProfile.Name + '-' + $householdRepeat
        $householdCsv = Join-Path $householdOutput ($householdName + '.csv')
        $householdLog = Join-Path $householdOutput ($householdName + '.log')
        $householdErrors = Join-Path $householdOutput ($householdName + '-errors.log')
        Write-Output ('Starting ' + $householdName)
        $householdProcess = Start-Process -FilePath $householdExecutable -ArgumentList @(
            $householdMode, $householdProfile.Name, ('"' + $householdProfile.Level + '"'), ('"' + $householdCsv + '"')
        ) -WorkingDirectory $householdRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $householdLog -RedirectStandardError $householdErrors
        $householdNativeHandle = $householdProcess.Handle
        try {
            for ($householdAttempt = 0; $householdAttempt -lt 150; ++$householdAttempt) {
                $householdProcess.Refresh()
                if ($householdProcess.HasExited -or $householdProcess.MainWindowHandle -ne 0) { break }
                Start-Sleep -Milliseconds 20
            }
            if (!$householdProcess.HasExited -and $householdProcess.MainWindowHandle -ne 0) {
                [void][HouseholdMeasureDesktop]::ShowWindow($householdProcess.MainWindowHandle, 9)
                [void][HouseholdMeasureDesktop]::SetForegroundWindow($householdProcess.MainWindowHandle)
            }
            while (!$householdProcess.WaitForExit(1000)) { }
            $householdExit = $householdProcess.ExitCode
        } finally { $householdProcess.Dispose() }
        $householdLogText = Get-Content -Raw -LiteralPath $householdLog
        $householdValid = $householdExit -eq 0 -and ($DebugBuild -or $householdLogText -match 'Validation disabled; FIFO;')
        $householdRuns += [ordered]@{ name = $householdName; exit_code = $householdExit; workload_pass = $householdValid; csv = $householdCsv }
        [System.IO.File]::WriteAllText((Join-Path $householdOutput 'runs.json'), ($householdRuns | ConvertTo-Json -Depth 6), $householdUtf8)
        if (!$householdValid) { throw ('Measurement failed: ' + $householdName + '. Partial timing CSV, workload evidence and logs retained; no acceptance claimed.') }
        $householdCsvFiles += $householdCsv
        Write-Output ('Completed ' + $householdName)
    }
}
if ($Check) { Write-Output 'Functional workload/recovery checks retained; no performance acceptance.'; return }
$householdSummaryText = & python (Join-Path $PSScriptRoot 'summarize_lighting_timings.py') @householdCsvFiles
if ($LASTEXITCODE -ne 0) { throw 'Timing summary failed; raw samples retained.' }
[System.IO.File]::WriteAllLines((Join-Path $householdOutput 'summary.json'), [string[]]$householdSummaryText, $householdUtf8)
$householdSummaries = ($householdSummaryText -join [Environment]::NewLine) | ConvertFrom-Json
$householdFailures = @()
foreach ($householdSummary in $householdSummaries) {
    foreach ($householdGate in $householdSummary.gates.PSObject.Properties) {
        if ($householdGate.Value -ne 'pass') { $householdFailures += ($householdSummary.path + ': ' + $householdGate.Name + '=' + $householdGate.Value) }
    }
    if ($householdSummary.non_submitted_sample_rows -ne 0 -or $householdSummary.framebuffer_sizes.Count -ne 1 -or
        $householdSummary.framebuffer_sizes[0] -ne '1920x1080' -or $householdSummary.last_sample_seconds -lt 69) {
        $householdFailures += ($householdSummary.path + ': incomplete duration/submission/framebuffer coverage')
    }
}
[System.IO.File]::WriteAllText((Join-Path $householdOutput 'acceptance.json'),
    ([ordered]@{ pass = ($householdFailures.Count -eq 0); failed_or_unavailable = $householdFailures; samples = $householdCsvFiles.Count } | ConvertTo-Json -Depth 6), $householdUtf8)
if ($householdFailures.Count -ne 0) { throw ('Failed/unavailable gates retained in summary.json and acceptance.json: ' + ($householdFailures -join '; ')) }
Write-Output 'All six runs meet the declared physical workload and T1 timing gates. Raw per-run evidence and machine configuration retained.'
