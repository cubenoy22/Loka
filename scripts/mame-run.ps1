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
$bootDisk = if ($env:MAME_BOOT_HDA) { $env:MAME_BOOT_HDA } else {
    Join-Path $ProjectDirectory "build/mame-run/Boot.hd"
}
New-Item -ItemType Directory -Path $mameHome -Force | Out-Null
New-Item -ItemType Directory -Path $controlDirectory -Force | Out-Null

# Mirrors mame-run.sh: MAME writes back to whatever it boots, so never hand it
# MAME_HDA itself. That image is the Classic rail's template, and the pixels the
# goldens are made of live inside it. Boot a copy under build/ instead. The copy
# persists so an interactive session keeps its state; wiping build/ resets it.
function Resolve-FileIdentity([string]$Path) {
    # Same path spelled twice, and one symlink hop. NTFS hard links share a file
    # id that Windows PowerShell cannot read without extra tooling, so that case
    # is caught by the shell twin's -ef test rather than here.
    $full = [System.IO.Path]::GetFullPath($Path)
    $item = Get-Item -LiteralPath $full -Force -ErrorAction SilentlyContinue
    if ($item -and $item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
        $target = $item.Target
        if ($target) {
            $full = [System.IO.Path]::GetFullPath($target)
        }
    }
    return $full
}

if ($env:MAME_HDA) {
    if (-not (Test-Path -LiteralPath $env:MAME_HDA -PathType Leaf)) {
        throw "boot hard disk template not found: $($env:MAME_HDA)"
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $bootDisk) -Force | Out-Null
    # An alias defeats the whole point: the existence check would pass and
    # -hard1 would name the template after all.
    if ((Test-Path -LiteralPath $bootDisk) -and
        ((Resolve-FileIdentity $bootDisk) -ieq (Resolve-FileIdentity $env:MAME_HDA))) {
        throw "MAME_BOOT_HDA resolves to the boot template itself: $bootDisk"
    }
    # Copy through a temporary and rename, so an interrupted copy cannot leave a
    # truncated image that every later run would find, skip the copy for, and
    # boot.
    if (-not (Test-Path -LiteralPath $bootDisk -PathType Leaf)) {
        $partial = "$bootDisk.partial"
        try {
            Copy-Item -LiteralPath $env:MAME_HDA -Destination $partial -Force
            Move-Item -LiteralPath $partial -Destination $bootDisk -Force
        } catch {
            Remove-Item -LiteralPath $partial -Force -ErrorAction SilentlyContinue
            throw
        }
    }
    # Unconditional, not part of the copy: a copy this run did not make can be
    # read-only too -- one an earlier launcher left behind, or one restored from
    # a read-only source. MAME needs to write to it either way, and normalising
    # the attribute leaves the persisted session state untouched.
    Set-ItemProperty -LiteralPath $bootDisk -Name IsReadOnly -Value $false
}
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
    $mameArguments += @("-hard1", $bootDisk)
}

$mameArguments += @("-scsi:5", "harddisk")
if (Test-Path -LiteralPath $developmentDisk) {
    $mameArguments += @("-hard2", (Resolve-Path -LiteralPath $developmentDisk).Path)
}

$mameArguments += @(
    "-autoboot_script", (Join-Path $ScriptDirectory "mame-floppy-service.lua")
)

# Mirrors mame-run.sh: MAME_DEBUG=1 halts at reset with the gdbstub listening
# on MAME_DEBUG_PORT (default 23946) until a gdb connects and continues.
if ($env:MAME_DEBUG) {
    $debugPort = if ($env:MAME_DEBUG_PORT) { $env:MAME_DEBUG_PORT } else { "23946" }
    $mameArguments += @("-debug", "-debugger", "gdbstub", "-debugger_port", $debugPort)
}

& $mameExecutable @mameArguments
exit $LASTEXITCODE
