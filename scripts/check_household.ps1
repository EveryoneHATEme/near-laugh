param(
    [string]$OutputDirectory,
    [switch]$SkipBuild,
    [switch]$Vulkan
)
$ErrorActionPreference = 'Stop'
$householdRoot = Split-Path -Parent $PSScriptRoot
if (!$OutputDirectory) {
    $OutputDirectory = Join-Path $householdRoot ('build/household-check-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
}
if (![System.IO.Path]::IsPathRooted($OutputDirectory)) {
    $OutputDirectory = Join-Path (Get-Location).Path $OutputDirectory
}
$householdOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $householdOutput) { throw 'Choose a fresh check directory to preserve earlier results.' }
$householdCmake = (Get-Command cmake -CommandType Application -ErrorAction Stop).Source
$householdCtest = (Get-Command ctest -CommandType Application -ErrorAction Stop).Source
[void](New-Item -ItemType Directory -Path $householdOutput)
$householdSteps = New-Object 'System.Collections.Generic.List[object]'
$householdStarted = [DateTime]::UtcNow.ToString('o')
$householdExitCode = 1
$householdFailure = $null

function Invoke-HouseholdCheckStep([string]$Name, [string]$Executable, [string[]]$Arguments) {
    $stepLog = Join-Path $householdOutput ($Name + '.log')
    Write-Output ($Name + ' ...')
    # Windows PowerShell can turn native stderr into ErrorRecords even when
    # the executable succeeds. Preserve both streams and use its real exit code.
    $ErrorActionPreference = 'Continue'
    & $Executable @Arguments 2>&1 | Out-File -LiteralPath $stepLog -Encoding UTF8
    $stepExit = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    $householdSteps.Add([ordered]@{
        name = $Name
        executable = $Executable
        arguments = $Arguments
        exit_code = $stepExit
        log = $stepLog
    })
    if ($stepExit -ne 0) {
        $script:householdExitCode = $stepExit
        Get-Content -LiteralPath $stepLog -Tail 25
        throw ($Name + ' failed with exit code ' + $stepExit + '; see ' + $stepLog)
    }
    Write-Output ($Name + ': passed')
}

Push-Location $householdRoot
try {
    if (!$SkipBuild) {
        Invoke-HouseholdCheckStep 'configure' $householdCmake @('--preset', 'debug')
        # Build the preset's process/resource probes as well as engine_tests.
        Invoke-HouseholdCheckStep 'build' $householdCmake @('--build', '--preset', 'debug', '--parallel', '4')
    }
    # The debug preset excludes every vulkan-smoke test. Real ImGui tests use
    # an in-memory context; household_smoke_fixtures performs CPU preflight only.
    Invoke-HouseholdCheckStep 'debug-tests' $householdCtest @(
        '--preset', 'debug', '--output-on-failure', '--no-tests=error', '--parallel', '1',
        '--output-junit', (Join-Path $householdOutput 'debug-tests.xml')
    )
    if ($Vulkan) {
        # Explicit opt-in: these automated readbacks create native windows,
        # but do not send desktop mouse or keyboard input.
        Invoke-HouseholdCheckStep 'vulkan-tests' $householdCtest @(
            '--preset', 'vulkan-smoke', '--output-on-failure', '--no-tests=error', '--parallel', '1',
            '--output-junit', (Join-Path $householdOutput 'vulkan-tests.xml')
        )
    }
    $householdExitCode = 0
} catch {
    $householdFailure = $_.Exception.Message
    Write-Warning $householdFailure
} finally {
    Pop-Location
    $householdReport = [ordered]@{
        started_utc = $householdStarted
        finished_utc = [DateTime]::UtcNow.ToString('o')
        mode = $(if ($Vulkan) { 'automated-with-vulkan-windows' } else { 'headless' })
        build_requested = !$SkipBuild
        passed = ($householdExitCode -eq 0)
        exit_code = $householdExitCode
        failure = $householdFailure
        steps = @($householdSteps.ToArray())
        unverified = @('subjective hold/throw feel', 'physical audio listening')
    }
    $householdUtf8 = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText((Join-Path $householdOutput 'result.json'), ($householdReport | ConvertTo-Json -Depth 8), $householdUtf8)
    Write-Output ('Results: ' + $householdOutput)
}
exit $householdExitCode
