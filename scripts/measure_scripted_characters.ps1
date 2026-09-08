param([Parameter(Mandatory=$true)][string]$OutputDirectory, [switch]$Check, [switch]$DebugBuild,
      [ValidateSet(0,1,4)][int[]]$Counts = @(0,1,4), [ValidateRange(1,3)][int]$Repeats = 3)
$ErrorActionPreference = 'Stop'
$p07Root = Split-Path -Parent $PSScriptRoot
$p07Output = [System.IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $p07Output) { throw 'Choose a fresh measurement directory.' }
$p07Build = if ($DebugBuild) { 'debug' } else { 'p10-release' }
$p07Executable = Join-Path $p07Root ('build/' + $p07Build + '/bin/scripted_character_measure.exe')
if (!(Test-Path -LiteralPath $p07Executable)) { throw 'Build the Release scripted_character_measure target first.' }
[void](New-Item -ItemType Directory -Path $p07Output)
$p07Utf8 = New-Object System.Text.UTF8Encoding($false)
if (!('ScriptedMeasureDesktop' -as [type])) { Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ScriptedMeasureDesktop {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int command);
}
'@
}
$p07Repeats = if ($Check) { 1 } else { $Repeats }
$p07Mode = if ($Check) { '--check' } else { '--measure' }
foreach ($p07Count in $Counts) {
    for ($p07Repeat = 1; $p07Repeat -le $p07Repeats; ++$p07Repeat) {
        $p07Name = 'scripted-' + $p07Count + '-' + $p07Repeat
        Write-Output ('Starting ' + $p07Name)
        $p07Csv = Join-Path $p07Output ($p07Name + '.csv')
        $p07Process = Start-Process -FilePath $p07Executable -ArgumentList @($p07Mode, $p07Count, ('"' + $p07Csv + '"')) -WorkingDirectory $p07Root -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $p07Output ($p07Name + '.log')) -RedirectStandardError (Join-Path $p07Output ($p07Name + '-errors.log'))
        $p07NativeHandle = $p07Process.Handle
        try {
            for ($p07Attempt = 0; $p07Attempt -lt 100; ++$p07Attempt) {
                $p07Process.Refresh()
                if ($p07Process.HasExited -or $p07Process.MainWindowHandle -ne 0) { break }
                Start-Sleep -Milliseconds 20
            }
            if (!$p07Process.HasExited) {
                [void][ScriptedMeasureDesktop]::ShowWindow($p07Process.MainWindowHandle, 9)
                [void][ScriptedMeasureDesktop]::SetForegroundWindow($p07Process.MainWindowHandle)
            }
            while (!$p07Process.WaitForExit(1000)) { }
            $p07Exit = $p07Process.ExitCode
        } finally { $p07Process.Dispose() }
        $p07Log = Get-Content -LiteralPath (Join-Path $p07Output ($p07Name + '.log'))
        if ($p07Exit -ne 0) { throw ('Measurement failed: ' + $p07Name + '. Partial CSV/log retained.') }
        if (!$DebugBuild -and !($p07Log -match 'Validation disabled; FIFO;')) { throw 'Release validation-off confirmation is missing.' }
        Write-Output ('Completed ' + $p07Name)
    }
}
if ($Check) { Write-Output 'Functional timing/recovery checks retained; not performance samples.'; return }
$p07CsvFiles = Get-ChildItem -LiteralPath $p07Output -Filter '*.csv' | Select-Object -ExpandProperty FullName
$p07Summary = & python (Join-Path $PSScriptRoot 'summarize_lighting_timings.py') @p07CsvFiles
if ($LASTEXITCODE -ne 0) { throw 'Timing summary failed.' }
[System.IO.File]::WriteAllLines((Join-Path $p07Output 'summary.json'), [string[]]$p07Summary, $p07Utf8)
Write-Output 'Samples retained. Review all timing gates and route, physics, pose, audio, deformation and upload costs.'
