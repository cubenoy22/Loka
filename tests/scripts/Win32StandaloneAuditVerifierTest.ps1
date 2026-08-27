param(
    [Parameter(Mandatory = $true)]
    [string]$FixtureExecutable
)

$ErrorActionPreference = "Stop"

$ProjectDirectory = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Subject = Join-Path $ProjectDirectory "scripts/win32-standalone-flow.ps1"
$TestRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
    ("loka-win32-standalone-audit-" + [Guid]::NewGuid())
$Catalog = @(
    @("scrapbook", "LokaScrapbookStandaloneFlowWin32", "scrapbook/standalone-tour.audit"),
    @("helloworld", "LokaHelloWorldStandaloneFlowWin32", "helloworld/toggle-action-probe.audit"),
    @("tutorial", "LokaTutorialStandaloneFlowWin32", "tutorial/increment-summary-toggle.audit"),
    @("minesweeper", "LokaMineSweeperStandaloneFlowWin32", "minesweeper/new-game-twice.audit"),
    @("floppybird", "LokaFloppyBirdStandaloneFlowWin32", "floppybird/fixed-step-flaps.audit")
)

function Invoke-Verifier {
    $previousFixtureDirectory = $env:LOKA_STANDALONE_AUDIT_FIXTURE_DIR
    $previousErrorAction = $ErrorActionPreference
    try {
        $env:LOKA_STANDALONE_AUDIT_FIXTURE_DIR = Join-Path $TestRoot "fixtures"
        $ErrorActionPreference = "Continue"
        $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass `
            -File (Join-Path $TestRoot "Verify-StandaloneFlow.ps1") `
            -Action Verify -TimeoutSeconds 5 2>&1
        return @{
            ExitCode = $LASTEXITCODE
            Output = ($output | Out-String)
        }
    } finally {
        $ErrorActionPreference = $previousErrorAction
        $env:LOKA_STANDALONE_AUDIT_FIXTURE_DIR = $previousFixtureDirectory
    }
}

function Write-Catalog([int]$EntryCount = 5) {
    $lines = @("# SimpleViewer is excluded from automation: interactive file chooser.")
    for ($index = 0; $index -lt $EntryCount; ++$index) {
        $entry = $Catalog[$index]
        $lines += ($entry[0] + "`t" + $entry[1] + "`t" +
            "tests/scenarios/expected/" + $entry[2])
    }
    [System.IO.File]::WriteAllLines(
        (Join-Path $TestRoot "standalone-flow-catalog.tsv"), $lines)
}

function Mutate-Verdict([string]$Path, [string]$Prefix, [string]$Replacement) {
    $lines = [System.IO.File]::ReadAllLines($Path)
    $found = $false
    for ($index = 0; $index -lt $lines.Length; ++$index) {
        if ($lines[$index].StartsWith($Prefix)) {
            $lines[$index] = $Replacement
            $found = $true
            break
        }
    }
    if (-not $found) {
        throw "Fixture has no verdict line beginning with '$Prefix': $Path"
    }
    $utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText(
        $Path, ([string]::Join("`n", $lines) + "`n"), $utf8WithoutBom)
}

try {
    $expectedRoot = Join-Path $TestRoot "expected"
    $fixtureRoot = Join-Path $TestRoot "fixtures"
    New-Item -ItemType Directory -Path $expectedRoot, $fixtureRoot | Out-Null
    Copy-Item -LiteralPath $Subject `
        -Destination (Join-Path $TestRoot "Verify-StandaloneFlow.ps1")
    [System.IO.File]::WriteAllBytes(
        (Join-Path $TestRoot "ASSETS.LRP"),
        [System.Text.Encoding]::ASCII.GetBytes("fixture-assets"))
    Write-Catalog

    foreach ($entry in $Catalog) {
        $trackedAudit = Join-Path $ProjectDirectory `
            ("tests/scenarios/expected/" + $entry[2])
        Copy-Item -LiteralPath $FixtureExecutable `
            -Destination (Join-Path $TestRoot ($entry[1] + ".exe"))
        Copy-Item -LiteralPath $trackedAudit `
            -Destination (Join-Path $expectedRoot ($entry[0] + ".audit"))
        Copy-Item -LiteralPath $trackedAudit `
            -Destination (Join-Path $fixtureRoot ($entry[0] + ".audit"))
    }

    $accepted = Invoke-Verifier
    if ($accepted.ExitCode -ne 0) {
        throw "The tracked five-app catalog was refused:`n$($accepted.Output)"
    }
    if (($accepted.Output.Split("`n") | Where-Object {
            $_ -match "Runtime-verified Win32 Standalone Flow:"
        }).Count -ne 5) {
        throw "The verifier did not report all five applications:`n$($accepted.Output)"
    }
    foreach ($entry in $Catalog) {
        $actualAudit = Join-Path (Join-Path $TestRoot "actual") `
            ($entry[0] + ".audit")
        if (-not (Test-Path -LiteralPath $actualAudit)) {
            throw "The verifier did not retain actual evidence for $($entry[0])."
        }
    }

    $tutorialFixture = Join-Path $fixtureRoot "tutorial.audit"
    Mutate-Verdict $tutorialFixture "text.value`t" "text.value`tbogus"
    $refused = Invoke-Verifier
    if ($refused.ExitCode -eq 0 `
        -or $refused.Output -notmatch "does not match the tracked expected audit") {
        throw "The bogus verdict body failed for the wrong reason:`n$($refused.Output)"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $TestRoot "actual/tutorial.audit")) `
        -or (Test-Path -LiteralPath (Join-Path $TestRoot "actual/minesweeper.audit"))) {
        throw "The verifier did not stop at the first mismatching application."
    }

    Write-Catalog 4
    $missingEntry = Invoke-Verifier
    if ($missingEntry.ExitCode -eq 0 `
        -or $missingEntry.Output -notmatch "must contain five runnable applications") {
        throw "The incomplete catalog failed for the wrong reason:`n$($missingEntry.Output)"
    }

    Write-Catalog
    $missingExecutable = Join-Path $TestRoot `
        "LokaFloppyBirdStandaloneFlowWin32.exe"
    Remove-Item -LiteralPath $missingExecutable -Force
    Remove-Item -LiteralPath (Join-Path $TestRoot "actual") `
        -Recurse -Force -ErrorAction SilentlyContinue
    $incompleteStage = Invoke-Verifier
    if ($incompleteStage.ExitCode -eq 0 `
        -or $incompleteStage.Output -notmatch "Staged executable not found") {
        throw "The incomplete Stage failed for the wrong reason:`n$($incompleteStage.Output)"
    }
    if (Test-Path -LiteralPath (Join-Path $TestRoot "actual")) {
        throw "The verifier launched an application before completing Stage preflight."
    }
    Copy-Item -LiteralPath $FixtureExecutable -Destination $missingExecutable

    Copy-Item -LiteralPath (Join-Path $expectedRoot "tutorial.audit") `
        -Destination $tutorialFixture -Force
    $restored = Invoke-Verifier
    if ($restored.ExitCode -ne 0) {
        throw "The restored catalog was refused:`n$($restored.Output)"
    }
    $scrapbookFixture = Join-Path $fixtureRoot "scrapbook.audit"
    Mutate-Verdict $scrapbookFixture `
        "view.target.present`t" "view.target.present`t0"
    $failedRerun = Invoke-Verifier
    if ($failedRerun.ExitCode -eq 0) {
        throw "The verifier accepted a bad first application on rerun."
    }
    foreach ($key in @("helloworld", "tutorial", "minesweeper", "floppybird")) {
        if (Test-Path -LiteralPath (Join-Path $TestRoot ("actual/" + $key + ".audit"))) {
            throw "A failed rerun retained stale later evidence for $key."
        }
    }

    Write-Output "Win32 five-app verifier validates the catalog and exact audit bytes."
} finally {
    Remove-Item -LiteralPath $TestRoot -Recurse -Force -ErrorAction SilentlyContinue
}
