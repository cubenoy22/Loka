$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2

$ProjectDirectory = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$ProfileSupport = Join-Path $ProjectDirectory "tests/win32/ScenarioProfile.ps1"
$TestRoot = Join-Path ([System.IO.Path]::GetTempPath()) `
    ("loka-win32-scenario-profile-" + [Guid]::NewGuid())

function Write-Profile([string]$Path, [string[]]$Lines) {
    $utf8WithoutBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText(
        $Path, ([string]::Join("`n", $Lines) + "`n"), $utf8WithoutBom)
}

. $ProfileSupport

try {
    New-Item -ItemType Directory -Path $TestRoot | Out-Null
    $expectedPath = Join-Path $TestRoot "expected.profile"
    $actualPath = Join-Path $TestRoot "actual.profile"
    $profileLines = @(
        "profile_version=2",
        "os_build=10.0.26100",
        "arch=x64",
        "scale_percent_available=1",
        "scale_percent=100",
        "depth_available=1",
        "depth=32",
        "appearance_available=1",
        "appearance=light",
        "capture_api=PrintWindow.PW_RENDERFULLCONTENT.v1",
        "pixel_width=800",
        "pixel_height=600"
    )

    Write-Profile $expectedPath $profileLines
    Write-Profile $actualPath $profileLines
    $expected = Read-ScenarioProfile $expectedPath
    $actual = Read-ScenarioProfile $actualPath
    $mismatch = Get-CaptureProfileMismatch $expected $actual
    if ($null -ne $mismatch) {
        throw "Identical profiles were refused at field '$mismatch'."
    }

    $archLines = @($profileLines | ForEach-Object {
        if ($_ -eq "arch=x64") { "arch=arm64" } else { $_ }
    })
    Write-Profile $actualPath $archLines
    $actual = Read-ScenarioProfile $actualPath
    $mismatch = Get-CaptureProfileMismatch $expected $actual
    if ($mismatch -cne "arch") {
        throw "An arch-only change reported '$mismatch' instead of 'arch'."
    }

    $futureLines = @($profileLines) + "future_capture_fact=present"
    Write-Profile $actualPath $futureLines
    $actual = Read-ScenarioProfile $actualPath
    $mismatch = Get-CaptureProfileMismatch $expected $actual
    if ($mismatch -cne "future_capture_fact") {
        throw "An actual-only schema field reported '$mismatch' instead of 'future_capture_fact'."
    }

    Write-Profile $expectedPath $futureLines
    Write-Profile $actualPath $profileLines
    $expected = Read-ScenarioProfile $expectedPath
    $actual = Read-ScenarioProfile $actualPath
    $mismatch = Get-CaptureProfileMismatch $expected $actual
    if ($mismatch -cne "future_capture_fact") {
        throw "An expected-only schema field reported '$mismatch' instead of 'future_capture_fact'."
    }

    Write-Output "Win32 scenario profiles compare every field in either profile."
} finally {
    Remove-Item -LiteralPath $TestRoot -Recurse -Force -ErrorAction SilentlyContinue
}
