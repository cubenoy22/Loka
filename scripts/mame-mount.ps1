param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$DiskImage
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

$mameHome = if ($env:MAME_HOMEPATH) { $env:MAME_HOMEPATH } else {
    Join-Path $HOME ".mame"
}
$controlDirectory = if ($env:MAME_CONTROL_DIR) { $env:MAME_CONTROL_DIR } else {
    Join-Path $mameHome "loka"
}
$requestPath = Join-Path $controlDirectory "floppy.request"
$responsePath = Join-Path $controlDirectory "floppy.response"

New-Item -ItemType Directory -Path $controlDirectory -Force | Out-Null
Remove-Item -LiteralPath $responsePath -Force -ErrorAction SilentlyContinue

$requestTemporary = "$requestPath.$PID"
if ($DiskImage -eq "--eject" -or $DiskImage -eq "eject") {
    [IO.File]::WriteAllText($requestTemporary, "eject`n")
} else {
    $resolvedDiskImage = (Resolve-Path -LiteralPath $DiskImage).Path
    [IO.File]::WriteAllText($requestTemporary, "mount`n$resolvedDiskImage`n")
}
Move-Item -LiteralPath $requestTemporary -Destination $requestPath -Force

for ($attempt = 0; $attempt -lt 100; $attempt++) {
    if (Test-Path -LiteralPath $responsePath) {
        $response = Get-Content -LiteralPath $responsePath
        Remove-Item -LiteralPath $responsePath -Force
        if ($response[0] -eq "ok") {
            Write-Output $response[1]
            exit 0
        }
        throw "MAME floppy service error: $($response[1])"
    }
    Start-Sleep -Milliseconds 100
}

throw "MAME floppy service did not respond; start MAME first"
