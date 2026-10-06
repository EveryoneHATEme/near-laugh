param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [switch]$Check,
    [switch]$DebugBuild
)
$ErrorActionPreference = 'Stop'
$narrativeRoot = Split-Path -Parent $PSScriptRoot
$narrativeOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $narrativeOutput) { throw 'Choose a fresh measurement directory.' }
if ($DebugBuild -and !$Check) { throw 'Performance samples require the separate Release build.' }
$narrativeBuild = if ($DebugBuild) { 'debug' } else { 'p10-release' }
$narrativeExecutable = Join-Path $narrativeRoot ('build/' + $narrativeBuild + '/bin/narrative_measure.exe')
$narrativeLevel = Join-Path $narrativeRoot 'resources/levels/narrative-t4.level.json'
if (!(Test-Path -LiteralPath $narrativeExecutable)) { throw 'Build narrative_measure in the selected tree first.' }

# Explicit invocation opens a fullscreen window. Use an isolated measurement
# desktop or obtain authorization for this desktop run. This script does not
# build targets, stop unrelated applications, or change timing acceptance gates.
function Assert-NarrativeMeasurementIdle {
    $narrativeConflicts = @(Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $_.ProcessName -match '^(cmake|ninja|ctest|clang.*|cl|link|lld.*|engine_tests|near_laugh|level_editor|narrative_fixture|household_measure|interior_lighting_measure|scripted_character_measure|character_animation_viewer|character_animation_smoke|.*_smoke|.*_measure)$'
    } | Select-Object ProcessName, Id)
    if ($narrativeConflicts.Count -ne 0) {
        $narrativeConflictText = ($narrativeConflicts | ForEach-Object { $_.ProcessName + '(' + $_.Id + ')' }) -join ', '
        throw ('Concurrent build/test/game/GPU workload detected: ' + $narrativeConflictText)
    }
}
Assert-NarrativeMeasurementIdle
[void](New-Item -ItemType Directory -Path $narrativeOutput)
$narrativeUtf8 = New-Object System.Text.UTF8Encoding($false)
$narrativePreflight = Join-Path $narrativeOutput 'profiles'
$narrativePreflightLog = Join-Path $narrativeOutput 'preflight.log'
& $narrativeExecutable --preflight $narrativeLevel $narrativePreflight *> $narrativePreflightLog
if ($LASTEXITCODE -ne 0) { throw 'No-window profile preflight failed; diagnostic log retained.' }
$narrativeMetadata = [ordered]@{
    started_utc = [DateTime]::UtcNow.ToString('o')
    mode = $(if ($Check) { 'check-not-performance' } else { 'measure' })
    build = $narrativeBuild
    executable = $narrativeExecutable
    executable_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $narrativeExecutable).Hash
    source_level = $narrativeLevel
    source_level_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $narrativeLevel).Hash
    profiles = @('disabled', 'capacity') | ForEach-Object {
        $narrativeProfilePath = Join-Path $narrativePreflight ($_ + '.level.json')
        [ordered]@{ name = $_; path = $narrativeProfilePath; sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $narrativeProfilePath).Hash }
    }
    resolution = '1920x1080'
    refresh_hz = 60
    present_mode = 'FIFO'
    warmup_seconds = 10
    capture_seconds = 60
    repeats_per_workload = $(if ($Check) { 1 } else { 3 })
    pair_order = @('disabled/capacity', 'capacity/disabled', 'disabled/capacity')
    audio_output = 'silent'
    workload = '32 facts;32 regions;64 rearming events;32 immediate steps/event;8 predicates/list;accepted radio toggle every 0.5s;at least55 complete event cycles during capture'
    comparison = 'Same T4 scene/camera/radio stimulus; capacity desired-state no-op commands retain identical idle actors, static lights/doors and external audio workload'
    scopes = 'Existing FrameTimings fields and unchanged T1 gates; narrative execution included in total active CPU; workload assertions outside active CPU'
    process_check_scope = 'Known compiler/build/test/game/measurement processes; other GPU applications must be idle in the authorized test environment'
    cpu = @(Get-CimInstance Win32_Processor | Select-Object Name, NumberOfCores, NumberOfLogicalProcessors, MaxClockSpeed)
    gpu = @(Get-CimInstance Win32_VideoController | Select-Object Name, DriverVersion, AdapterRAM, CurrentHorizontalResolution, CurrentVerticalResolution, CurrentRefreshRate)
    os = @(Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber)
}
[System.IO.File]::WriteAllText((Join-Path $narrativeOutput 'configuration.json'), ($narrativeMetadata | ConvertTo-Json -Depth 12), $narrativeUtf8)

if (!('NarrativeMeasureDesktop' -as [type])) { Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class NarrativeMeasureDesktop {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int command);
}
'@
}
$narrativeRepeats = if ($Check) { 1 } else { 3 }
$narrativeMode = if ($Check) { '--check' } else { '--measure' }
$narrativeCsvFiles = @()
$narrativeRuns = @()
for ($narrativeRepeat = 1; $narrativeRepeat -le $narrativeRepeats; ++$narrativeRepeat) {
    $narrativeOrder = if ($narrativeRepeat % 2 -eq 1) { @('disabled', 'capacity') } else { @('capacity', 'disabled') }
    foreach ($narrativeProfile in $narrativeOrder) {
        Assert-NarrativeMeasurementIdle
        if ((Get-FileHash -Algorithm SHA256 -LiteralPath $narrativeExecutable).Hash -ne $narrativeMetadata.executable_sha256 -or
            (Get-FileHash -Algorithm SHA256 -LiteralPath $narrativeLevel).Hash -ne $narrativeMetadata.source_level_sha256) {
            throw 'Executable or source scene changed during paired measurements.'
        }
        $narrativeName = $narrativeProfile + '-' + $narrativeRepeat
        $narrativeCsv = Join-Path $narrativeOutput ($narrativeName + '.csv')
        $narrativeLog = Join-Path $narrativeOutput ($narrativeName + '.log')
        $narrativeErrors = Join-Path $narrativeOutput ($narrativeName + '-errors.log')
        $narrativeEvidencePath = Join-Path $narrativeOutput ($narrativeName + '.workload.json')
        Write-Output ('Starting ' + $narrativeName)
        $narrativeProcess = Start-Process -FilePath $narrativeExecutable -ArgumentList @(
            $narrativeMode, $narrativeProfile, ('"' + $narrativeLevel + '"'), ('"' + $narrativeCsv + '"')
        ) -WorkingDirectory $narrativeRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $narrativeLog -RedirectStandardError $narrativeErrors
        $narrativeNativeHandle = $narrativeProcess.Handle
        try {
            for ($narrativeAttempt = 0; $narrativeAttempt -lt 150; ++$narrativeAttempt) {
                $narrativeProcess.Refresh()
                if ($narrativeProcess.HasExited -or $narrativeProcess.MainWindowHandle -ne 0) { break }
                Start-Sleep -Milliseconds 20
            }
            if (!$narrativeProcess.HasExited -and $narrativeProcess.MainWindowHandle -ne 0) {
                [void][NarrativeMeasureDesktop]::ShowWindow($narrativeProcess.MainWindowHandle, 9)
                [void][NarrativeMeasureDesktop]::SetForegroundWindow($narrativeProcess.MainWindowHandle)
            }
            while (!$narrativeProcess.WaitForExit(1000)) { }
            $narrativeExit = $narrativeProcess.ExitCode
        } finally { $narrativeProcess.Dispose() }
        $narrativeLogText = Get-Content -Raw -LiteralPath $narrativeLog
        $narrativeValid = $narrativeExit -eq 0 -and ($DebugBuild -or $narrativeLogText -match 'Validation disabled; FIFO;')
        $narrativeEvidence = $null
        if (Test-Path -LiteralPath $narrativeEvidencePath) {
            $narrativeEvidence = Get-Content -Raw -LiteralPath $narrativeEvidencePath | ConvertFrom-Json
            $narrativeExpectedCount = if ($narrativeProfile -eq 'capacity') { 64 } else { 0 }
            $narrativeValid = $narrativeValid -and $narrativeEvidence.passed -eq $true -and
                $narrativeEvidence.events -eq $narrativeExpectedCount -and $narrativeEvidence.radio_on_starts -gt 0
        } else { $narrativeValid = $false }
        $narrativeRuns += [ordered]@{ name = $narrativeName; pair = $narrativeRepeat; exit_code = $narrativeExit; workload_pass = $narrativeValid; csv = $narrativeCsv; workload = $narrativeEvidencePath }
        [System.IO.File]::WriteAllText((Join-Path $narrativeOutput 'runs.json'), ($narrativeRuns | ConvertTo-Json -Depth 6), $narrativeUtf8)
        if (!$narrativeValid) { throw ('Measurement failed: ' + $narrativeName + '. Partial timing CSV, workload evidence and logs retained; no acceptance claimed.') }
        $narrativeCsvFiles += $narrativeCsv
        Write-Output ('Completed ' + $narrativeName)
    }
}
if ($Check) { Write-Output 'Functional workload/recovery checks retained; no performance acceptance.'; return }
$narrativeSummaryText = & python (Join-Path $PSScriptRoot 'summarize_lighting_timings.py') @narrativeCsvFiles
if ($LASTEXITCODE -ne 0) { throw 'Timing summary failed; raw samples retained.' }
[System.IO.File]::WriteAllLines((Join-Path $narrativeOutput 'summary.json'), [string[]]$narrativeSummaryText, $narrativeUtf8)
$narrativeSummaries = ($narrativeSummaryText -join [Environment]::NewLine) | ConvertFrom-Json
$narrativeFailures = @()
foreach ($narrativeSummary in $narrativeSummaries) {
    foreach ($narrativeGate in $narrativeSummary.gates.PSObject.Properties) {
        if ($narrativeGate.Value -ne 'pass') { $narrativeFailures += ($narrativeSummary.path + ': ' + $narrativeGate.Name + '=' + $narrativeGate.Value) }
    }
    if ($narrativeSummary.non_submitted_sample_rows -ne 0 -or $narrativeSummary.framebuffer_sizes.Count -ne 1 -or
        $narrativeSummary.framebuffer_sizes[0] -ne '1920x1080' -or $narrativeSummary.last_sample_seconds -lt 69) {
        $narrativeFailures += ($narrativeSummary.path + ': incomplete duration/submission/framebuffer coverage')
    }
}
[System.IO.File]::WriteAllText((Join-Path $narrativeOutput 'acceptance.json'),
    ([ordered]@{ pass = ($narrativeFailures.Count -eq 0); failed_or_unavailable = $narrativeFailures; samples = $narrativeCsvFiles.Count; pairs = $narrativeRepeats } | ConvertTo-Json -Depth 6), $narrativeUtf8)
if ($narrativeFailures.Count -ne 0) { throw ('Failed/unavailable gates retained in summary.json and acceptance.json: ' + ($narrativeFailures -join '; ')) }
Write-Output 'All six runs meet the executed narrative workload and unchanged T1 timing gates. Raw per-run evidence and machine configuration retained.'
