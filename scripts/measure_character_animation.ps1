param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$p07Root = Split-Path -Parent $PSScriptRoot
$p07Output = [System.IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $p07Output) { throw 'Choose a fresh measurement directory.' }
$p07Executable = Join-Path $p07Root 'build/p10-release/bin/character_animation_viewer.exe'
if (!(Test-Path -LiteralPath $p07Executable)) { throw 'Build the Release character_animation_viewer target first.' }
[void](New-Item -ItemType Directory -Path $p07Output)
$p07Utf8 = New-Object System.Text.UTF8Encoding($false)
foreach ($p07Count in @(0, 1, 4)) {
    for ($p07Repeat = 1; $p07Repeat -le 3; ++$p07Repeat) {
        $p07Name = 'characters-' + $p07Count + '-' + $p07Repeat
        Write-Output ('Starting ' + $p07Name)
        $p07Csv = Join-Path $p07Output ($p07Name + '.csv')
        $ErrorActionPreference = 'Continue'
        try {
            $p07Log = & $p07Executable --measure $p07Count $p07Csv 2>&1
            $p07Exit = $LASTEXITCODE
        } finally { $ErrorActionPreference = 'Stop' }
        [System.IO.File]::WriteAllLines((Join-Path $p07Output ($p07Name + '.log')), [string[]]$p07Log, $p07Utf8)
        if ($p07Exit -ne 0) { throw ('Measurement failed: ' + $p07Name + '. Partial CSV/log retained.') }
        if (!($p07Log -match 'Validation disabled; FIFO;')) { throw 'Release validation-off confirmation is missing.' }
        Write-Output ('Completed ' + $p07Name)
    }
}
$p07CsvFiles = Get-ChildItem -LiteralPath $p07Output -Filter '*.csv' | Select-Object -ExpandProperty FullName
$p07Summary = & python (Join-Path $PSScriptRoot 'summarize_lighting_timings.py') @p07CsvFiles
if ($LASTEXITCODE -ne 0) { throw 'Timing summary failed.' }
[System.IO.File]::WriteAllLines((Join-Path $p07Output 'summary.json'), [string[]]$p07Summary, $p07Utf8)
Write-Output 'Nine samples retained. Review all timing gates and character deformation/upload costs.'
