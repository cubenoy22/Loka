param(
    [Parameter(Mandatory = $true)]
    [string]$FixtureExecutable
)

$ErrorActionPreference = "Stop"

$ProjectDirectory = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Subject = Join-Path $ProjectDirectory "scripts/win32-standalone-flow.ps1"
$ExpectedAudit = Join-Path $ProjectDirectory `
    "tests/scenarios/expected/scrapbook/standalone-tour.audit"
$TestRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
    ("loka-win32-standalone-audit-" + [Guid]::NewGuid())

function Invoke-Verifier([string]$AuditFixture) {
    $previousFixture = $env:LOKA_STANDALONE_AUDIT_FIXTURE
    $previousErrorAction = $ErrorActionPreference
    try {
        $env:LOKA_STANDALONE_AUDIT_FIXTURE = $AuditFixture
        $ErrorActionPreference = "Continue"
        $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass `
            -File (Join-Path $TestRoot "Verify-StandaloneFlow.ps1") `
            -Action Verify 2>&1
        return @{
            ExitCode = $LASTEXITCODE
            Output = ($output | Out-String)
        }
    } finally {
        $ErrorActionPreference = $previousErrorAction
        $env:LOKA_STANDALONE_AUDIT_FIXTURE = $previousFixture
    }
}

try {
    New-Item -ItemType Directory -Path $TestRoot | Out-Null
    Copy-Item -LiteralPath $Subject `
        -Destination (Join-Path $TestRoot "Verify-StandaloneFlow.ps1")
    Copy-Item -LiteralPath $FixtureExecutable `
        -Destination (Join-Path $TestRoot "LokaScrapbookStandaloneFlowWin32.exe")
    Copy-Item -LiteralPath $ExpectedAudit `
        -Destination (Join-Path $TestRoot "standalone-tour.audit")
    [System.IO.File]::WriteAllBytes(
        (Join-Path $TestRoot "ASSETS.LRP"),
        [System.Text.Encoding]::ASCII.GetBytes("fixture-assets"))

    $accepted = Invoke-Verifier $ExpectedAudit
    if ($accepted.ExitCode -ne 0) {
        throw "The tracked audit was refused:`n$($accepted.Output)"
    }

    $lines = [System.IO.File]::ReadAllLines($ExpectedAudit)
    for ($index = 0; $index -lt 20; ++$index) {
        $lines[13 + $index] = "bogus_{0:D2}" -f ($index + 1)
    }
    $bogus = Join-Path $TestRoot "bogus.audit"
    $utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText(
        $bogus, ([string]::Join("`n", $lines) + "`n"), $utf8WithoutBom)

    $refused = Invoke-Verifier $bogus
    if ($refused.ExitCode -eq 0) {
        throw "The verifier accepted a bogus verdict body."
    }
    if ($refused.Output -notmatch "does not match the tracked expected audit") {
        throw "The bogus verdict body failed for the wrong reason:`n$($refused.Output)"
    }

    Write-Output "Win32 standalone verifier accepts only the tracked audit bytes."
} finally {
    Remove-Item -LiteralPath $TestRoot -Recurse -Force -ErrorAction SilentlyContinue
}
