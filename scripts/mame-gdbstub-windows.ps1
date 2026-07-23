param(
    [int]$Port = 23946,
    [int]$StartupTimeoutSeconds = 30,
    [Parameter(Mandatory = $true)]
    [string]$OwnerPidFile
)

$ErrorActionPreference = "Stop"

$ScriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$MameLauncher = Join-Path $ScriptDirectory "mame-run.ps1"
$launcherProcess = $null

$env:MAME_DEBUG = "1"
$env:MAME_DEBUG_PORT = $Port.ToString()

function Test-GdbStubListener([int]$ExpectedPort) {
    $listeners = [Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveTcpListeners()
    foreach ($listener in $listeners) {
        $isLocalAddress = [Net.IPAddress]::IsLoopback($listener.Address) -or
            $listener.Address.Equals([Net.IPAddress]::Any) -or
            $listener.Address.Equals([Net.IPAddress]::IPv6Any)
        if ($isLocalAddress -and $listener.Port -eq $ExpectedPort) {
            return $true
        }
    }
    return $false
}

try {
    $launcherArguments = "-NoProfile -ExecutionPolicy Bypass -File `"$MameLauncher`""
    $launcherProcess = Start-Process -FilePath "powershell.exe" `
        -ArgumentList $launcherArguments -PassThru -NoNewWindow
    [IO.File]::WriteAllText($OwnerPidFile, $launcherProcess.Id.ToString(),
        [Text.Encoding]::ASCII)

    $deadline = [DateTime]::UtcNow.AddSeconds($StartupTimeoutSeconds)
    while (-not (Test-GdbStubListener $Port)) {
        if ($launcherProcess.HasExited) {
            throw "Windows MAME launcher exited before the gdbstub opened port $Port."
        }
        if ([DateTime]::UtcNow -ge $deadline) {
            throw "Timed out waiting for the Windows MAME gdbstub on localhost:$Port."
        }
        Start-Sleep -Milliseconds 100
    }

    # cppdbg waits for this exact line before starting the WSL-side GDB transport.
    # Bypass PowerShell's formatting pipeline so the readiness line is immediate.
    $readyLine = "LOKA_MAME_GDBSTUB_READY localhost:$Port`n"
    $readyBytes = [Text.Encoding]::ASCII.GetBytes($readyLine)
    $standardOutput = [Console]::OpenStandardOutput()
    $standardOutput.Write($readyBytes, 0, $readyBytes.Length)
    $standardOutput.Flush()
    $launcherProcess.WaitForExit()
    exit $launcherProcess.ExitCode
} finally {
    if ($launcherProcess -and -not $launcherProcess.HasExited) {
        & taskkill.exe /PID $launcherProcess.Id /T /F 2>$null | Out-Null
    }
}
