param(
    [ValidateSet("Build", "Stage", "Verify")]
    [string]$Action = "Verify",

    [ValidateSet("x86", "x64", "arm64")]
    [string]$Architecture,

    [string]$BuildDirectory,

    [string]$StageDirectory,

    [int]$TimeoutSeconds = 30
)

$ErrorActionPreference = "Stop"

$ScriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDirectory = Split-Path -Parent $ScriptDirectory
$packagedExecutable = Join-Path $ScriptDirectory "LokaScrapbookStandaloneFlowWin32.exe"
$isPackagedVerifier = Test-Path -LiteralPath $packagedExecutable

function Resolve-ProjectPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path $ProjectDirectory $Path
}

function Get-PeArchitecture([string]$Path) {
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
        switch ($machine) {
            0x014C { return "x86" }
            0x8664 { return "x64" }
            0xAA64 { return "arm64" }
            default { throw ("Unsupported PE machine type 0x{0:X4}: {1}" -f $machine, $Path) }
        }
    } finally {
        $stream.Dispose()
    }
}

function Assert-ExecutableArchitecture([string]$Path, [string]$Expected) {
    $actual = Get-PeArchitecture $Path
    if ($actual -ne $Expected) {
        throw (("Expected a {0} PE executable, but the selected compiler produced {1}. " +
            "Start VS Code from the Visual Studio Command Line Tools for the intended target and use a fresh architecture-specific preset.") -f
            $Expected, $actual)
    }
}

function Assert-FileCopy([string]$Source, [string]$Destination) {
    $sourceFile = Get-Item -LiteralPath $Source
    $destinationFile = Get-Item -LiteralPath $Destination
    if ($sourceFile.Length -ne $destinationFile.Length `
        -or (Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash `
            -ne (Get-FileHash -LiteralPath $Destination -Algorithm SHA256).Hash) {
        throw "Staged file does not match its source: $Destination"
    }
}

function Get-CompilerEnvironmentArchitecture {
    $compiler = Get-Command cl.exe -ErrorAction SilentlyContinue
    if (-not $compiler) {
        throw "cl.exe is not available. Start VS Code from Visual Studio Command Line Tools for the intended target."
    }

    if (-not $env:VSCMD_ARG_TGT_ARCH) {
        throw ("The Visual Studio target architecture is unavailable. " +
            "Start VS Code from ARM64, x64, or x86 Visual Studio Command Line Tools.")
    }

    $target = $env:VSCMD_ARG_TGT_ARCH.ToLowerInvariant()
    if ($target -eq "i386") {
        $target = "x86"
    }
    if ($target -notin @("arm64", "x64", "x86")) {
        throw "Unsupported Visual Studio target architecture: $target"
    }
    return $target
}

function Assert-CompilerEnvironment([string]$Expected) {
    $target = Get-CompilerEnvironmentArchitecture
    if ($target -ne $Expected) {
        throw (("The requested architecture is {0}, but this Visual Studio environment targets {1}. " +
            "Restart VS Code from the intended Visual Studio Command Line Tools or omit -Architecture to use the inherited target.") -f
            $Expected, $target)
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
    if ($lines.Count -ne 34) {
        throw "Expected 34 audit lines, found $($lines.Count)."
    }
    if ($lines[0] -ne "loka_scenario_audit version=1 scenario=standalone-tour") {
        throw "Unexpected audit header: $($lines[0])"
    }
    for ($step = 1; $step -le 12; ++$step) {
        if ($lines[$step] -notmatch "^step id=$step .* status=succeeded(?: |$)") {
            throw "Unexpected audit step ${step}: $($lines[$step])"
        }
    }
    if ($lines[33] -ne "terminal status=succeeded") {
        throw "Unexpected audit terminal: $($lines[33])"
    }
}

if (-not $Architecture) {
    if ($isPackagedVerifier) {
        $Architecture = Get-PeArchitecture $packagedExecutable
    } else {
        $Architecture = Get-CompilerEnvironmentArchitecture
    }
}

if ($isPackagedVerifier -and $Action -ne "Verify") {
    throw "The staged verifier supports only the Verify action."
}

$configurePreset = "win32-$Architecture-release"
$buildPreset = "win32-standalone-flow-$Architecture-release"
if (-not $BuildDirectory) {
    $BuildDirectory = "build/win32/presentation/$Architecture/Release"
}
if (-not $StageDirectory) {
    $StageDirectory = "build/presentation/win32-$Architecture-release"
}

$buildRoot = Resolve-ProjectPath $BuildDirectory
$stageRoot = if ($isPackagedVerifier) { $ScriptDirectory } else { Resolve-ProjectPath $StageDirectory }
$builtExecutable = Join-Path $buildRoot "example/ScrapbookUI/standalone-flow/LokaScrapbookStandaloneFlowWin32.exe"
$builtAssets = Join-Path $buildRoot "example/ScrapbookUI/standalone-flow/ASSETS.LRP"
$stagedExecutable = Join-Path $stageRoot "LokaScrapbookStandaloneFlowWin32.exe"
$stagedAssets = Join-Path $stageRoot "ASSETS.LRP"
$stagedVerifier = Join-Path $stageRoot "Verify-StandaloneFlow.ps1"
$auditPath = Join-Path $stageRoot "LOG.TXT"

if (-not $isPackagedVerifier) {
    Assert-CompilerEnvironment $Architecture
    Push-Location $ProjectDirectory
    try {
        & cmake --preset $configurePreset
        if ($LASTEXITCODE) {
            throw "CMake configure failed with exit code $LASTEXITCODE."
        }
        & cmake --build --preset $buildPreset
        if ($LASTEXITCODE) {
            throw "CMake build failed with exit code $LASTEXITCODE."
        }
    } finally {
        Pop-Location
    }

    if (-not (Test-Path -LiteralPath $builtExecutable)) {
        throw "Standalone Flow executable not found: $builtExecutable"
    }
    if (-not (Test-Path -LiteralPath $builtAssets)) {
        throw "Standalone Flow assets not found: $builtAssets"
    }
    Assert-ExecutableArchitecture $builtExecutable $Architecture

    if ($Action -eq "Build") {
        Write-Output "Built $Architecture Standalone Flow: $builtExecutable"
        exit 0
    }

    $presentationStageHelper = Join-Path $ScriptDirectory "presentation-stage.ps1"
    . $presentationStageHelper
    $sourceVerifier = $MyInvocation.MyCommand.Path
    $populateStage = {
        param([string]$Destination)

        $destinationExecutable = Join-Path $Destination "LokaScrapbookStandaloneFlowWin32.exe"
        $destinationAssets = Join-Path $Destination "ASSETS.LRP"
        $destinationVerifier = Join-Path $Destination "Verify-StandaloneFlow.ps1"
        Copy-Item -LiteralPath $builtExecutable -Destination $destinationExecutable
        Copy-Item -LiteralPath $builtAssets -Destination $destinationAssets
        Copy-Item -LiteralPath $sourceVerifier -Destination $destinationVerifier
        Assert-ExecutableArchitecture $destinationExecutable $Architecture
        Assert-FileCopy $builtExecutable $destinationExecutable
        Assert-FileCopy $builtAssets $destinationAssets
        Assert-FileCopy $sourceVerifier $destinationVerifier
    }.GetNewClosure()
    Install-LokaPresentationStageDirectory -StageRoot $stageRoot -Populate $populateStage

    if ($Action -eq "Stage") {
        Write-Output "Staged $Architecture presentation: $stageRoot"
        exit 0
    }
}

if (-not (Test-Path -LiteralPath $stagedExecutable)) {
    throw "Staged executable not found: $stagedExecutable."
}
if (-not (Test-Path -LiteralPath $stagedAssets)) {
    throw "Staged assets not found: $stagedAssets."
}
Assert-ExecutableArchitecture $stagedExecutable $Architecture
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
                Write-Output "Runtime-verified $Architecture Standalone Flow: $auditPath"
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
