param(
    [ValidateSet("Stage", "Verify")]
    [string]$Action = "Verify",

    [string]$BuildDirectory = "build/win32/x86/Release",

    [string]$StageDirectory,

    [int]$TimeoutSeconds = 30
)

$ErrorActionPreference = "Stop"

$ScriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDirectory = Split-Path -Parent $ScriptDirectory

function Resolve-ProjectPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path $ProjectDirectory $Path
}

function Assert-X86Executable([string]$Path) {
    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    try {
        $reader = New-Object System.IO.BinaryReader($stream)
        if ($reader.ReadUInt16() -ne 0x5A4D) {
            throw "Not a PE executable: $Path"
        }
        $stream.Position = 0x3C
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) {
            throw "Invalid PE signature: $Path"
        }
        $machine = $reader.ReadUInt16()
        if ($machine -ne 0x014C) {
            throw (("Expected an x86 PE executable for the VAIO P, but machine type is 0x{0:X4}. " +
                "Start VS Code from the VS2017 x64_x86 Cross Tools prompt and rebuild.") -f $machine)
        }
    } finally {
        $stream.Dispose()
    }
}

function Read-SharedText([string]$Path) {
    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    try {
        $reader = New-Object System.IO.StreamReader($stream)
        return $reader.ReadToEnd()
    } finally {
        $stream.Dispose()
    }
}

function Assert-SuccessAudit([string]$Content) {
    $lines = @($Content -split "`r?`n" | Where-Object { $_ -ne "" })
    if ($lines.Count -ne 14) {
        throw "Expected 14 audit lines, found $($lines.Count)."
    }
    if ($lines[0] -ne "loka_scenario_audit version=1 scenario=standalone-tour") {
        throw "Unexpected audit header: $($lines[0])"
    }
    for ($step = 1; $step -le 12; ++$step) {
        if ($lines[$step] -notmatch "^step id=$step .* status=succeeded(?: |$)") {
            throw "Unexpected audit step ${step}: $($lines[$step])"
        }
    }
    if ($lines[13] -ne "terminal status=succeeded") {
        throw "Unexpected audit terminal: $($lines[13])"
    }
}

$buildRoot = Resolve-ProjectPath $BuildDirectory
if ($StageDirectory) {
    $stageRoot = Resolve-ProjectPath $StageDirectory
} elseif (Test-Path -LiteralPath (Join-Path $ScriptDirectory "LokaScrapbookStandaloneFlowWin32.exe")) {
    $stageRoot = $ScriptDirectory
} else {
    $stageRoot = Join-Path $ProjectDirectory "build/presentation/win32-x86-release"
}
$builtExecutable = Join-Path $buildRoot "win32/LokaScrapbookStandaloneFlowWin32.exe"
$builtAssets = Join-Path $buildRoot "win32/ASSETS.LRP"
$stagedExecutable = Join-Path $stageRoot "LokaScrapbookStandaloneFlowWin32.exe"
$stagedAssets = Join-Path $stageRoot "ASSETS.LRP"
$stagedVerifier = Join-Path $stageRoot "Verify-StandaloneFlow.ps1"
$auditPath = Join-Path $stageRoot "LOG.TXT"

if ($Action -eq "Stage") {
    if (-not (Test-Path -LiteralPath $builtExecutable)) {
        throw "Standalone Flow executable not found: $builtExecutable"
    }
    if (-not (Test-Path -LiteralPath $builtAssets)) {
        throw "Standalone Flow assets not found: $builtAssets"
    }
    Assert-X86Executable $builtExecutable
    New-Item -ItemType Directory -Path $stageRoot -Force | Out-Null
    Copy-Item -LiteralPath $builtExecutable -Destination $stagedExecutable -Force
    Copy-Item -LiteralPath $builtAssets -Destination $stagedAssets -Force
    Copy-Item -LiteralPath $MyInvocation.MyCommand.Path -Destination $stagedVerifier -Force
    Remove-Item -LiteralPath $auditPath -Force -ErrorAction SilentlyContinue
    Write-Output "Staged VAIO P presentation: $stageRoot"
    exit 0
}

if (-not (Test-Path -LiteralPath $stagedExecutable)) {
    throw "Staged executable not found: $stagedExecutable. Run the Stage task first."
}
if (-not (Test-Path -LiteralPath $stagedAssets)) {
    throw "Staged assets not found: $stagedAssets. Run the Stage task first."
}
Assert-X86Executable $stagedExecutable
Remove-Item -LiteralPath $auditPath -Force -ErrorAction SilentlyContinue

$process = $null
try {
    $process = Start-Process -FilePath $stagedExecutable -WorkingDirectory $stageRoot -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if (Test-Path -LiteralPath $auditPath) {
            $content = Read-SharedText $auditPath
            if ($content -match "(?m)^terminal status=(failed|canceled)\r?$") {
                throw "Standalone Flow reported terminal status $($Matches[1])."
            }
            if ($content -match "(?m)^terminal status=succeeded\r?$") {
                Assert-SuccessAudit $content
                Write-Output "Runtime-verified x86 Standalone Flow: $auditPath"
                exit 0
            }
        }
        if ($process.HasExited) {
            throw "Standalone Flow exited before publishing a success audit (exit code $($process.ExitCode))."
        }
        Start-Sleep -Milliseconds 200
    }
    throw "Timed out after $TimeoutSeconds seconds waiting for Standalone Flow success."
} finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit()
    }
}
