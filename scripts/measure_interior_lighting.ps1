param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$p10Root = Split-Path -Parent $PSScriptRoot
$p10Output = [System.IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $p10Output) { throw 'Choose a fresh measurement directory.' }
[void](New-Item -ItemType Directory -Path $p10Output)
$p10Executable = Join-Path $p10Root 'build/p10-release/bin/interior_lighting_measure.exe'
if (!(Test-Path -LiteralPath $p10Executable)) { throw 'Build the separate Release measurement target first.' }
$p10Utf8 = New-Object System.Text.UTF8Encoding($false)
$p10Scenes = @(
    @{ Name = 'six'; File = 'interior-lighting' },
    @{ Name = 'eight'; File = 'interior-lighting-capacity' }
)
foreach ($p10Scene in $p10Scenes) {
    $p10Source = Join-Path $p10Root ('resources/levels/' + $p10Scene.File + '.level.json')
    $p10Document = Get-Content -Raw -Encoding UTF8 -LiteralPath $p10Source | ConvertFrom-Json
    foreach ($p10Light in $p10Document.environment_light.point_lights) { $p10Light.casts_shadows = $false }
    $p10Baseline = Join-Path $p10Output ($p10Scene.Name + '-unshadowed.level.json')
    [System.IO.File]::WriteAllText($p10Baseline, ($p10Document | ConvertTo-Json -Depth 100), $p10Utf8)
    foreach ($p10Kind in @('shadowed', 'unshadowed')) {
        $p10Level = if ($p10Kind -eq 'shadowed') { $p10Source } else { $p10Baseline }
        $p10Repeats = if ($p10Kind -eq 'shadowed') { 3 } else { 1 }
        foreach ($p10Mode in @('stationary', 'route')) {
            for ($p10Repeat=1; $p10Repeat -le $p10Repeats; ++$p10Repeat) {
                $p10Name = $p10Scene.Name + '-' + $p10Kind + '-' + $p10Mode + '-' + $p10Repeat
                Write-Output ('Starting ' + $p10Name)
                $p10Csv = Join-Path $p10Output ($p10Name + '.csv')
                # Native stderr must be retained even when the child exits
                # unsuccessfully; Windows PowerShell otherwise terminates
                # this assignment before writing the diagnostic log.
                $ErrorActionPreference = 'Continue'
                try {
                    $p10Log = & $p10Executable $p10Level $p10Mode $p10Csv 2>&1
                    $p10Exit = $LASTEXITCODE
                } finally { $ErrorActionPreference = 'Stop' }
                [System.IO.File]::WriteAllLines((Join-Path $p10Output ($p10Name + '.log')), [string[]]$p10Log, $p10Utf8)
                if ($p10Exit -ne 0) { throw ('Measurement failed: ' + $p10Name + '. Partial CSV/log retained.') }
                if (!($p10Log -match 'Validation disabled; FIFO;')) { throw 'Release validation-off confirmation is missing.' }
                Write-Output ('Completed ' + $p10Name)
            }
        }
    }
}
$p10CsvFiles = Get-ChildItem -LiteralPath $p10Output -Filter '*.csv' | Select-Object -ExpandProperty FullName
$p10Summary = & python (Join-Path $PSScriptRoot 'summarize_lighting_timings.py') @p10CsvFiles
if ($LASTEXITCODE -ne 0) { throw 'Timing summary failed.' }
[System.IO.File]::WriteAllLines((Join-Path $p10Output 'summary.json'), [string[]]$p10Summary, $p10Utf8)
Write-Output 'All 12 shadowed samples and four equivalent unshadowed baselines captured. Review every gate in summary.json.'
