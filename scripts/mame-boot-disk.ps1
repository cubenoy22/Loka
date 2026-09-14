param(
    [switch]$All,

    [Parameter(Position = 0)]
    [string]$MacBinaryPath,

    [Parameter(Position = 1, ValueFromRemainingArguments = $true)]
    [string[]]$PlainDataPaths = @()
)

$ErrorActionPreference = "Stop"

$ScriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDirectory = Split-Path -Parent $ScriptDirectory
$EnvironmentFile = if ($env:MAME_ENV_FILE) {
    $env:MAME_ENV_FILE
} else {
    Join-Path $ProjectDirectory ".env-mame"
}

if (Test-Path -LiteralPath $EnvironmentFile) {
    foreach ($line in Get-Content -LiteralPath $EnvironmentFile) {
        $trimmed = $line.Trim()
        if (-not $trimmed -or $trimmed.StartsWith("#")) {
            continue
        }
        if ($trimmed -notmatch "^([A-Za-z_][A-Za-z0-9_]*)=(.*)$") {
            throw "Invalid environment line in ${EnvironmentFile}: $line"
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

function Find-Retro68Tool([string]$Name) {
    $executableName = if ($IsWindows -or $env:OS -eq "Windows_NT") {
        "$Name.exe"
    } else {
        $Name
    }
    $candidates = @()
    if ($env:RETRO68_TOOLCHAIN_BIN) {
        $candidates += Join-Path $env:RETRO68_TOOLCHAIN_BIN $executableName
    }
    if ($env:RETRO68_BUILD_DIR) {
        $candidates += Join-Path $env:RETRO68_BUILD_DIR "toolchain/bin/$executableName"
    }
    $candidates += Join-Path $HOME "Retro68-build/toolchain/bin/$executableName"
    $candidates += Join-Path $HOME "Documents/Projects/Retro68-build/toolchain/bin/$executableName"

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }
    $command = Get-Command $executableName -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }
    throw "Retro68 tool not found: $Name"
}

function Resolve-FileIdentity([string]$Path) {
    # Mirrors mame-run.ps1: the .source record it compares against holds the
    # resolved template path, so the record written here must resolve the
    # same way or the next launch treats the copy as "template changed".
    $full = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
    $item = Get-Item -LiteralPath $full -Force -ErrorAction SilentlyContinue
    while ($item -and ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
        $target = $item.Target
        if (-not $target) { break }
        if (-not [System.IO.Path]::IsPathRooted($target)) {
            $target = Join-Path (Split-Path -Parent $full) $target
        }
        $full = [System.IO.Path]::GetFullPath($target)
        $item = Get-Item -LiteralPath $full -Force -ErrorAction SilentlyContinue
    }
    return [System.IO.Path]::GetFullPath($full).TrimEnd([char[]]"\/")
}

$machine = if ($env:MAME_MACHINE) { $env:MAME_MACHINE } else { "macplus" }
$mameHome = if ($env:MAME_HOMEPATH) { $env:MAME_HOMEPATH } else {
    Join-Path $HOME ".mame"
}
$controlDirectory = if ($env:MAME_CONTROL_DIR) { $env:MAME_CONTROL_DIR } else {
    Join-Path $mameHome "loka"
}
$bootDisk = if ($env:MAME_BOOT_HDA) { $env:MAME_BOOT_HDA } else {
    Join-Path $ProjectDirectory "build/mame-run/$machine/Boot.hd"
}
$bootDisk = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($bootDisk)

$macBinaries = @()
$plainData = @()

if ($All) {
    $releaseBuildRoot = Join-Path $ProjectDirectory "build/retro68/68k/Release/example"
    $allApps = @(
        "HelloWorld/LokaHello68K.bin",
        "MineSweeper/LokaMine68K.bin",
        "SimpleViewer/LokaSimpleViewer68K.bin",
        "FloppyBird/LokaFloppyBird68K.bin",
        "SmirkBench/LokaSmirkBench68K.bin",
        "LazyList/LokaLazyList68K.bin",
        "Tutorial/LokaTutorial68K.bin",
        "ScrapbookUI/ScrapbookUI68K.bin"
    )
    foreach ($rel in $allApps) {
        $p = Join-Path $releaseBuildRoot $rel
        if (-not (Test-Path -LiteralPath $p)) {
            throw "Application binary not found: $p (build Retro68 68K first)"
        }
        $macBinaries += (Resolve-Path -LiteralPath $p).Path
    }
    $assets = Join-Path $ProjectDirectory "example/ScrapbookUI/ASSETS.LRP"
    if (Test-Path -LiteralPath $assets) {
        $plainData += (Resolve-Path -LiteralPath $assets).Path
    }
} elseif ($MacBinaryPath) {
    $macBinaries += (Resolve-Path -LiteralPath $MacBinaryPath).Path
    foreach ($p in $PlainDataPaths) {
        $plainData += (Resolve-Path -LiteralPath $p).Path
    }
} else {
    throw "Usage: mame-boot-disk.ps1 -All | <MacBinaryPath> [PlainDataPaths ...]"
}

if (-not (Test-Path -LiteralPath $bootDisk)) {
    if (-not $env:MAME_HDA -or -not (Test-Path -LiteralPath $env:MAME_HDA)) {
        throw "MAME_HDA must point to the boot hard disk template: $($env:MAME_HDA)"
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $bootDisk) -Force | Out-Null
    $templatePath = Resolve-FileIdentity $env:MAME_HDA
    Copy-Item -LiteralPath $templatePath -Destination $bootDisk -Force
    Set-ItemProperty -LiteralPath $bootDisk -Name IsReadOnly -Value $false
    $templateSha = (Get-FileHash -LiteralPath $templatePath -Algorithm SHA256).Hash.ToLowerInvariant()
    [System.IO.File]::WriteAllText("$bootDisk.source", "$templatePath`n$templateSha`n",
        (New-Object System.Text.UTF8Encoding($false)))
}
Set-ItemProperty -LiteralPath $bootDisk -Name IsReadOnly -Value $false

$hfsHome = Join-Path $controlDirectory "hfsutils"
$hmount = Find-Retro68Tool "hmount"
$hcopy = Find-Retro68Tool "hcopy"
$hls = Find-Retro68Tool "hls"
$hmkdir = Find-Retro68Tool "hmkdir"
$humount = Find-Retro68Tool "humount"

New-Item -ItemType Directory -Path $hfsHome -Force | Out-Null

$originalHome = $env:HOME
try {
    $env:HOME = $hfsHome
    & $hmount $bootDisk
    if ($LASTEXITCODE) { throw "hmount failed with exit code $LASTEXITCODE" }

    $destDir = ":Loka:"
    & $hls -d ":Loka" 2>&1 | Out-Null
    if ($LASTEXITCODE) {
        & $hmkdir ":Loka"
    }

    foreach ($bin in $macBinaries) {
        & $hcopy -m $bin $destDir
        if ($LASTEXITCODE) { throw "hcopy failed for $bin with exit code $LASTEXITCODE" }
    }
    foreach ($data in $plainData) {
        & $hcopy -r $data $destDir
        if ($LASTEXITCODE) { throw "hcopy failed for $data with exit code $LASTEXITCODE" }
    }
    & $humount
    if ($LASTEXITCODE) { throw "humount failed with exit code $LASTEXITCODE" }
} finally {
    $env:HOME = $originalHome
}

if ($All) {
    Write-Output "Staged all 68K applications to boot disk ($destDir): $bootDisk"
} else {
    Write-Output "Staged to boot disk ($destDir): $bootDisk"
}
