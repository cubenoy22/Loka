$ErrorActionPreference = "Stop"

$ProjectDirectory = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Subject = Join-Path $ProjectDirectory "scripts/win32-standalone-flow.ps1"
$TestRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
    ("loka-win32-stage-" + [Guid]::NewGuid())
$BuildRoot = Join-Path $TestRoot "build"
$StageRoot = Join-Path $TestRoot "completed stage"
$ToolRoot = Join-Path $TestRoot "tools"
$Catalog = @(
    @("scrapbook", "LokaScrapbookStandaloneFlowWin32", "scrapbook/standalone-tour.audit"),
    @("helloworld", "LokaHelloWorldStandaloneFlowWin32", "helloworld/toggle-action-probe.audit"),
    @("tutorial", "LokaTutorialStandaloneFlowWin32", "tutorial/increment-summary-toggle.audit"),
    @("minesweeper", "LokaMineSweeperStandaloneFlowWin32", "minesweeper/new-game-twice.audit"),
    @("floppybird", "LokaFloppyBirdStandaloneFlowWin32", "floppybird/fixed-step-flaps.audit")
)

function New-TestPe([string]$Path, [byte]$Marker) {
    $bytes = New-Object byte[] 512
    $bytes[0] = 0x4D
    $bytes[1] = 0x5A
    [BitConverter]::GetBytes([int]0x80).CopyTo($bytes, 0x3C)
    $bytes[0x80] = 0x50
    $bytes[0x81] = 0x45
    [BitConverter]::GetBytes([uint16]0x8664).CopyTo($bytes, 0x84)
    $bytes[0x100] = $Marker
    [System.IO.File]::WriteAllBytes($Path, $bytes)
}

function Assert-BytesEqual([string]$Expected, [string]$Actual, [string]$Message) {
    $expectedBytes = [System.IO.File]::ReadAllBytes($Expected)
    $actualBytes = [System.IO.File]::ReadAllBytes($Actual)
    if ($expectedBytes.Length -ne $actualBytes.Length) {
        throw $Message
    }
    for ($index = 0; $index -lt $expectedBytes.Length; ++$index) {
        if ($expectedBytes[$index] -ne $actualBytes[$index]) {
            throw $Message
        }
    }
}

function Invoke-Stage {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Subject `
        -Action Stage -Architecture x64 `
        -BuildDirectory $BuildRoot -StageDirectory $StageRoot *> $null
    return $LASTEXITCODE
}

try {
    $builtDirectory = Join-Path $BuildRoot "standalone-flow"
    New-Item -ItemType Directory -Path `
        $builtDirectory, $StageRoot, $ToolRoot -Force | Out-Null

    $catalogLines = @(
        "# SimpleViewer is excluded from automation: interactive file chooser."
    )
    for ($index = 0; $index -lt $Catalog.Count; ++$index) {
        $entry = $Catalog[$index]
        New-TestPe (Join-Path $builtDirectory ($entry[1] + ".exe")) `
            ([byte](0x20 + $index))
        $catalogLines += ($entry[0] + "`t" + $entry[1] + "`t" +
            "tests/scenarios/expected/" + $entry[2])
    }
    $buildCatalog = Join-Path $BuildRoot "standalone-flow-catalog.tsv"
    [System.IO.File]::WriteAllLines($buildCatalog, $catalogLines)
    $builtAssets = Join-Path $builtDirectory "ASSETS.LRP"
    [System.IO.File]::WriteAllText($builtAssets, "new assets")
    $sentinel = Join-Path $StageRoot "completed-stage-sentinel.txt"
    [System.IO.File]::WriteAllText($sentinel, "old completed stage")

    Set-Content -LiteralPath (Join-Path $ToolRoot "cmake.cmd") -Value "@exit /b 0"
    Copy-Item -LiteralPath (Join-Path $env:SystemRoot "System32/where.exe") `
        -Destination (Join-Path $ToolRoot "cl.exe")

    $previousPath = $env:PATH
    $previousTarget = $env:VSCMD_ARG_TGT_ARCH
    $env:PATH = "$ToolRoot;$previousPath"
    $env:VSCMD_ARG_TGT_ARCH = "x64"
    $lockedAssets = [System.IO.File]::Open(
        $builtAssets,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::None)
    $previousErrorAction = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $stageExitCode = Invoke-Stage
    } finally {
        $ErrorActionPreference = $previousErrorAction
        $lockedAssets.Dispose()
    }

    if ($stageExitCode -eq 0) {
        throw "Stage unexpectedly succeeded while its assets source was locked."
    }
    if (-not (Test-Path -LiteralPath $sentinel) `
        -or [System.IO.File]::ReadAllText($sentinel) -ne "old completed stage") {
        throw "A failed Stage replaced the prior completed directory."
    }
    $transactionResidue = @(Get-ChildItem -LiteralPath $TestRoot -Force | Where-Object {
        $_.Name -like ".completed stage.*"
    })
    if ($transactionResidue.Count -ne 0) {
        throw "Failed Stage left transaction directories behind."
    }

    try {
        $stageExitCode = Invoke-Stage
    } finally {
        $env:PATH = $previousPath
        $env:VSCMD_ARG_TGT_ARCH = $previousTarget
    }
    if ($stageExitCode -ne 0) {
        throw "Completed Stage failed with exit code $stageExitCode."
    }
    if (Test-Path -LiteralPath $sentinel) {
        throw "Completed Stage retained a file from the replaced directory."
    }

    foreach ($entry in $Catalog) {
        Assert-BytesEqual `
            (Join-Path $builtDirectory ($entry[1] + ".exe")) `
            (Join-Path $StageRoot ($entry[1] + ".exe")) `
            "Completed Stage did not install $($entry[0])."
        Assert-BytesEqual `
            (Join-Path $ProjectDirectory ("tests/scenarios/expected/" + $entry[2])) `
            (Join-Path $StageRoot ("expected/" + $entry[0] + ".audit")) `
            "Completed Stage did not install the $($entry[0]) expected audit."
    }
    Assert-BytesEqual $builtAssets (Join-Path $StageRoot "ASSETS.LRP") `
        "Completed Stage did not install the built assets."
    Assert-BytesEqual $buildCatalog `
        (Join-Path $StageRoot "standalone-flow-catalog.tsv") `
        "Completed Stage did not install the generated catalog."
    Assert-BytesEqual $Subject (Join-Path $StageRoot "Verify-StandaloneFlow.ps1") `
        "Completed Stage did not install its verifier."

    $transactionResidue = @(Get-ChildItem -LiteralPath $TestRoot -Force | Where-Object {
        $_.Name -like ".completed stage.*"
    })
    if ($transactionResidue.Count -ne 0) {
        throw "Completed Stage left transaction directories behind."
    }

    Write-Output "Win32 five-app Stage is failure-atomic and portable."
} finally {
    Remove-Item -LiteralPath $TestRoot -Recurse -Force -ErrorAction SilentlyContinue
}
