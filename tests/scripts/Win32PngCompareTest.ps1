$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2

$ProjectDirectory = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$PngTool = Join-Path $ProjectDirectory "tests/scenarios/pngtool.py"
$PngCompareSupport = Join-Path $ProjectDirectory "tests/win32/PngCompare.ps1"
$TestRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
    ("loka-win32-png-compare-" + [Guid]::NewGuid())
$PythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
if (-not $PythonCommand) {
    throw "python.exe is required to exercise the Win32 PNG compare wrapper."
}
$Python = $PythonCommand.Source
$UseWslPython = $false
$PngExitCode = 0
$PngCompareResult = $null

function Fail-Stage([string]$StageName, [string]$Message) {
    throw "$StageName stage failed: $Message"
}

function Write-PngFixture([string]$Path, [string]$Payload) {
    [System.IO.File]::WriteAllBytes($Path, [Convert]::FromBase64String($Payload))
}

function Assert-CompareResult(
    [string]$Expected,
    [string]$Actual,
    [long]$ExpectedExitCode,
    [long]$ExpectedPixels,
    [long]$ExpectedColumns,
    [string]$Case
) {
    $output = Invoke-PngCompare $Expected $Actual 400 2 | Out-String
    if ($PngExitCode -ne $ExpectedExitCode) {
        throw "$Case returned $PngExitCode instead of $ExpectedExitCode.`n$output"
    }
    if ($null -eq $PngCompareResult) {
        throw "$Case did not return measured pixel and column counts.`n$output"
    }
    if ($PngCompareResult.DifferenceCount -ne $ExpectedPixels) {
        throw "$Case measured $($PngCompareResult.DifferenceCount) pixels instead of $ExpectedPixels.`n$output"
    }
    if ($PngCompareResult.DifferenceColumnCount -ne $ExpectedColumns) {
        throw "$Case measured $($PngCompareResult.DifferenceColumnCount) columns instead of $ExpectedColumns.`n$output"
    }
    if ($output -notlike "*max-diff-px: 400; max-diff-columns: 2;*") {
        throw "$Case did not report both tolerances.`n$output"
    }
}

. $PngCompareSupport

try {
    New-Item -ItemType Directory -Path $TestRoot | Out-Null
    $twoColumnExpected = Join-Path $TestRoot "two-column-expected.png"
    $twoColumnActual = Join-Path $TestRoot "two-column-actual.png"
    $threeColumnExpected = Join-Path $TestRoot "three-column-expected.png"
    $threeColumnActual = Join-Path $TestRoot "three-column-actual.png"

    Write-PngFixture $twoColumnExpected `
        "iVBORw0KGgoAAAANSUhEUgAAAAIAAACpCAIAAACOD3teAAAAGElEQVR4nGNgZGIGIoZRapQapUapQUYBAFDTB+0ndKTlAAAAAElFTkSuQmCC"
    Write-PngFixture $twoColumnActual `
        "iVBORw0KGgoAAAANSUhEUgAAAAIAAACpCAIAAACOD3teAAAAGElEQVR4nGPg5GAHIoZRapQapUapQUYBAEKyH7G2E3L/AAAAAElFTkSuQmCC"
    Write-PngFixture $threeColumnExpected `
        "iVBORw0KGgoAAAANSUhEUgAAAAMAAAABCAIAAACUgoPjAAAADUlEQVR4nGNgZGKGIAAAXgATTrM1MwAAAABJRU5ErkJggg=="
    Write-PngFixture $threeColumnActual `
        "iVBORw0KGgoAAAANSUhEUgAAAAMAAAABCAIAAACUgoPjAAAADUlEQVR4nGPg5GCHIAABeABJJXPVaQAAAABJRU5ErkJggg=="

    # The tracked worst case is two complete 169-pixel columns: 338 pixels.
    Assert-CompareResult $twoColumnExpected $twoColumnActual 0 338 2 `
        "The tracked two-column wobble"

    # Three pixels are far below the pixel ceiling, but their three-column shape
    # must refuse. This is the missing-glyph/content-loss discriminator.
    Assert-CompareResult $threeColumnExpected $threeColumnActual 1 3 3 `
        "A three-column content change"

    Write-Output "Win32 PNG compare enforces both pixel and column bounds."
} finally {
    Remove-Item -LiteralPath $TestRoot -Recurse -Force -ErrorAction SilentlyContinue
}
