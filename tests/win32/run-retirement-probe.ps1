param(
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [Parameter(Mandatory = $true)][string]$OutputDirectory,
    [string]$Configuration = 'Release',
    [ValidateRange(1, 3600)][int]$TimeoutSeconds = 60
)

$ErrorActionPreference = 'Stop'
$failed = $false
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$output = (Resolve-Path -LiteralPath $OutputDirectory).Path
# Accept the build root or the directory containing the executables. No
# console input, elevation or desktop creation: caller supplies the session.
foreach ($probe in @('Scrapbook', 'Dialog')) {
    $process = $null
    $log = $null
    $name = "LokaRetirementProbe${probe}Win32.exe"
    $logName = "retirement-probe-$($probe.ToLowerInvariant()).log"
    try {
        # Prevent a missing/crashed run from leaving an old output as evidence.
        $destination = Join-Path $output $logName
        if (Test-Path -LiteralPath $destination) { Remove-Item -LiteralPath $destination -Force }
        $candidates = @(
            (Join-Path $build $name),
            (Join-Path $build "standalone-flow/scrapbook/$Configuration/$name"),
            (Join-Path $build "standalone-flow/scrapbook/$name")
        )
        $exe = $candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
        if (-not $exe) { throw "Executable not found: $name" }
        $log = Join-Path (Split-Path -Parent $exe) $logName
        if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log -Force }
        $process = Start-Process -FilePath $exe -WorkingDirectory (Split-Path -Parent $exe) -PassThru
        $null = $process.Handle
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            $process.Kill()
            [void]$process.WaitForExit(5000)
            throw "$name timed out after $TimeoutSeconds seconds"
        }
        $process.Refresh()
        if ($process.ExitCode -ne 0) { throw "$name exited with code $($process.ExitCode)" }
        if (-not (Test-Path -LiteralPath $log)) { throw "$name produced no log" }
    }
    catch {
        Write-Output "error=$probe detail=$($_.Exception.Message)"
        $failed = $true
    }
    finally {
        try {
            if ($log -and (Test-Path -LiteralPath $log)) {
                if ([IO.Path]::GetFullPath($log) -ne [IO.Path]::GetFullPath((Join-Path $output $logName))) {
                    Copy-Item -LiteralPath $log -Destination (Join-Path $output $logName) -Force
                }
                Get-Content -LiteralPath $log | Where-Object { $_ -match '^summary ' } | Write-Output
            }
        }
        catch {
            Write-Output "error=${probe}-log detail=$($_.Exception.Message)"
            $failed = $true
        }
        if ($process) { $process.Dispose() }
    }
}
if ($failed) { exit 1 }
exit 0
