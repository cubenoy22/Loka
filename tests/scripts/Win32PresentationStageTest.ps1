$ErrorActionPreference = "Stop"

$ProjectDirectory = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Subject = Join-Path $ProjectDirectory "scripts/win32-standalone-flow.ps1"
$TestRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("loka-win32-stage-" + [Guid]::NewGuid())
$BuildRoot = Join-Path $TestRoot "build"
$StageRoot = Join-Path $TestRoot "completed stage"
$ToolRoot = Join-Path $TestRoot "tools"

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

try {
    $builtDirectory = Join-Path $BuildRoot "example/ScrapbookUI/standalone-flow"
    New-Item -ItemType Directory -Path $builtDirectory, $StageRoot, $ToolRoot -Force | Out-Null

    $builtExecutable = Join-Path $builtDirectory "LokaScrapbookStandaloneFlowWin32.exe"
    $builtAssets = Join-Path $builtDirectory "ASSETS.LRP"
    $oldExecutable = Join-Path $TestRoot "old.exe"
    $oldAssets = Join-Path $TestRoot "old-assets.lrp"
    $oldVerifier = Join-Path $TestRoot "old-verifier.ps1"
    New-TestPe $builtExecutable 0x22
    New-TestPe $oldExecutable 0x11
    [System.IO.File]::WriteAllText($builtAssets, "new assets")
    [System.IO.File]::WriteAllText($oldAssets, "old assets")
    [System.IO.File]::WriteAllText($oldVerifier, "old verifier")
    Copy-Item $oldExecutable (Join-Path $StageRoot "LokaScrapbookStandaloneFlowWin32.exe")
    Copy-Item $oldAssets (Join-Path $StageRoot "ASSETS.LRP")
    Copy-Item $oldVerifier (Join-Path $StageRoot "Verify-StandaloneFlow.ps1")

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
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Subject `
            -Action Stage -Architecture x64 `
            -BuildDirectory $BuildRoot -StageDirectory $StageRoot *> $null
        $stageExitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorAction
        $lockedAssets.Dispose()
        $env:PATH = $previousPath
        $env:VSCMD_ARG_TGT_ARCH = $previousTarget
    }

    if ($stageExitCode -eq 0) {
        throw "Stage unexpectedly succeeded while its assets source was locked."
    }
    Assert-BytesEqual $oldExecutable `
        (Join-Path $StageRoot "LokaScrapbookStandaloneFlowWin32.exe") `
        "A failed Stage replaced the completed executable."
    Assert-BytesEqual $oldAssets (Join-Path $StageRoot "ASSETS.LRP") `
        "A failed Stage replaced the completed assets."
    Assert-BytesEqual $oldVerifier (Join-Path $StageRoot "Verify-StandaloneFlow.ps1") `
        "A failed Stage replaced the completed verifier."

    $transactionResidue = @(Get-ChildItem -LiteralPath $TestRoot -Force | Where-Object {
        $_.Name -like ".completed stage.*"
    })
    if ($transactionResidue.Count -ne 0) {
        throw "Failed Stage left transaction directories behind."
    }

    $env:PATH = "$ToolRoot;$previousPath"
    $env:VSCMD_ARG_TGT_ARCH = "x64"
    try {
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Subject `
            -Action Stage -Architecture x64 `
            -BuildDirectory $BuildRoot -StageDirectory $StageRoot *> $null
        if ($LASTEXITCODE -ne 0) {
            throw "Completed Stage failed with exit code $LASTEXITCODE."
        }
    } finally {
        $env:PATH = $previousPath
        $env:VSCMD_ARG_TGT_ARCH = $previousTarget
    }

    Assert-BytesEqual $builtExecutable `
        (Join-Path $StageRoot "LokaScrapbookStandaloneFlowWin32.exe") `
        "Completed Stage did not install the built executable."
    Assert-BytesEqual $builtAssets (Join-Path $StageRoot "ASSETS.LRP") `
        "Completed Stage did not install the built assets."
    Assert-BytesEqual $Subject (Join-Path $StageRoot "Verify-StandaloneFlow.ps1") `
        "Completed Stage did not install its verifier."

    $transactionResidue = @(Get-ChildItem -LiteralPath $TestRoot -Force | Where-Object {
        $_.Name -like ".completed stage.*"
    })
    if ($transactionResidue.Count -ne 0) {
        throw "Completed Stage left transaction directories behind."
    }

    Write-Output "Win32 presentation Stage is failure-atomic and installs completed files."
} finally {
    Remove-Item -LiteralPath $TestRoot -Recurse -Force -ErrorAction SilentlyContinue
}
