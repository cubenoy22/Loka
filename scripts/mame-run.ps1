param(
    [string]$EnvironmentFile
)

$ErrorActionPreference = "Stop"

$ScriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDirectory = Split-Path -Parent $ScriptDirectory
if (-not $EnvironmentFile) {
    $EnvironmentFile = if ($env:MAME_ENV_FILE) {
        $env:MAME_ENV_FILE
    } else {
        Join-Path $ProjectDirectory ".env-mame"
    }
}

function Import-MameEnvironment([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        return
    }

    foreach ($line in Get-Content -LiteralPath $Path) {
        $trimmed = $line.Trim()
        if (-not $trimmed -or $trimmed.StartsWith("#")) {
            continue
        }
        if ($trimmed -notmatch "^([A-Za-z_][A-Za-z0-9_]*)=(.*)$") {
            throw "Invalid environment line in ${Path}: $line"
        }

        $name = $Matches[1]
        $value = $Matches[2].Trim()
        if (($value.StartsWith('"') -and $value.EndsWith('"')) -or
            ($value.StartsWith("'") -and $value.EndsWith("'"))) {
            $value = $value.Substring(1, $value.Length - 2)
        }
        $value = $value.Replace('${HOME}', $HOME).Replace('$HOME', $HOME)
        $value = [Environment]::ExpandEnvironmentVariables($value)
        Set-Item -Path "Env:$name" -Value $value
    }
}

Import-MameEnvironment $EnvironmentFile

$machine = if ($env:MAME_MACHINE) { $env:MAME_MACHINE } else { "maciici" }
$ramSize = if ($env:MAME_RAMSIZE) { $env:MAME_RAMSIZE } else { "8M" }
$mameHome = if ($env:MAME_HOMEPATH) { $env:MAME_HOMEPATH } else {
    Join-Path $HOME ".mame"
}
$mameExecutable = if ($env:MAME_EXECUTABLE) { $env:MAME_EXECUTABLE } else {
    "mame.exe"
}
$controlDirectory = if ($env:MAME_CONTROL_DIR) { $env:MAME_CONTROL_DIR } else {
    Join-Path $mameHome "loka"
}
$developmentDisk = if ($env:MAME_DEV_HDA) { $env:MAME_DEV_HDA } else {
    Join-Path $ProjectDirectory "build/mame-dev/LokaDev.hd"
}
New-Item -ItemType Directory -Path $mameHome -Force | Out-Null
New-Item -ItemType Directory -Path $controlDirectory -Force | Out-Null
$env:LOKA_MAME_FLOPPY_REQUEST = Join-Path $controlDirectory "floppy.request"
$env:LOKA_MAME_FLOPPY_RESPONSE = Join-Path $controlDirectory "floppy.response"

# Keep launcher policy aligned with mame-run.sh; only shell mechanics differ.
$mameArguments = @(
    $machine,
    "-ramsize", $ramSize,
    "-homepath", $mameHome,
    "-cfg_directory", (Join-Path $mameHome "cfg"),
    "-nvram_directory", (Join-Path $mameHome "nvram"),
    "-snapshot_directory", (Join-Path $mameHome "snap"),
    "-diff_directory", (Join-Path $mameHome "diff")
)

if ($env:MAME_ROMPATH) {
    $mameArguments += @("-rompath", $env:MAME_ROMPATH)
}
if ($env:MAME_HDA) {
    $mameArguments += @("-hard1", $env:MAME_HDA)
}

$mameArguments += @("-scsi:5", "harddisk")
if (Test-Path -LiteralPath $developmentDisk) {
    $mameArguments += @("-hard2", (Resolve-Path -LiteralPath $developmentDisk).Path)
}

$mameArguments += @(
    "-autoboot_script", (Join-Path $ScriptDirectory "mame-floppy-service.lua")
)

& $mameExecutable @mameArguments
exit $LASTEXITCODE
