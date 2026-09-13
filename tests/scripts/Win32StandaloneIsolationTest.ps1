param(
    [Parameter(Mandatory = $true)]
    [string]$StageRoot,
    [Parameter(Mandatory = $true)]
    [string]$EvidenceDirectory,
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"
$StageRoot = (Resolve-Path -LiteralPath $StageRoot).Path
$Applications = @(
    @{ Key = "scrapbook"; Target = "LokaScrapbookStandaloneLoopWin32"; Scenario = "standalone-tour" },
    @{ Key = "helloworld"; Target = "LokaHelloWorldStandaloneLoopWin32"; Scenario = "toggle-action-probe" }
)
$processes = @()

function Read-Audit([string]$Path) {
    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    try {
        $reader = New-Object System.IO.StreamReader($stream)
        try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
    } finally { $stream.Dispose() }
}

# This test opens two real app windows. Run in an interactive Windows session.
# Refuse existing audits rather than destroying evidence from another run.
foreach ($entry in $Applications) {
    $directory = Join-Path $StageRoot $entry.Key
    if (-not (Test-Path -LiteralPath (Join-Path $directory ($entry.Target + ".exe")))) {
        throw "Missing application: $($entry.Key)"
    }
    if (Test-Path -LiteralPath (Join-Path $directory "LOG.TXT")) {
        throw "Use a freshly staged package; $($entry.Key) already has LOG.TXT."
    }
}
if (Test-Path -LiteralPath (Join-Path $StageRoot "LOG.TXT")) {
    throw "Use a freshly staged package without a root LOG.TXT."
}
New-Item -ItemType Directory -Path $EvidenceDirectory -ErrorAction Stop | Out-Null

try {
    foreach ($entry in $Applications) {
        $executable = Join-Path (Join-Path $StageRoot $entry.Key) ($entry.Target + ".exe")
        # The SAME working directory deliberately cannot provide isolation.
        # Only each executable's application-sidecar directory can do so.
        $processes += Start-Process -FilePath $executable -WorkingDirectory $StageRoot -PassThru
    }
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        foreach ($process in $processes) {
            if ($process.HasExited) { throw "An application exited while both should remain running." }
        }
        if (Test-Path -LiteralPath (Join-Path $StageRoot "LOG.TXT")) {
            throw "An application wrote a shared working-directory audit."
        }
        $complete = 0
        foreach ($entry in $Applications) {
            $audit = Join-Path (Join-Path $StageRoot $entry.Key) "LOG.TXT"
            if (-not (Test-Path -LiteralPath $audit)) { continue }
            try { $content = Read-Audit $audit } catch [System.IO.IOException] { continue }
            if ($content -match "(?m)^terminal status=(failed|canceled)\r?$") {
                throw "$($entry.Key) reported a failed audit."
            }
            if ($content -match "(?m)^terminal status=succeeded\r?$") {
                if ($content -notmatch ("(?m)^loka_scenario_audit version=1 scenario=" +
                        [regex]::Escape($entry.Scenario) + "\r?$")) {
                    throw "$($entry.Key) received another application's audit."
                }
                ++$complete
            }
        }
        if ($complete -eq $Applications.Count) { break }
        Start-Sleep -Milliseconds 200
    } while ([DateTime]::UtcNow -lt $deadline)
    if ($complete -ne $Applications.Count) { throw "Timed out waiting for both independent audits." }
    foreach ($entry in $Applications) {
        Copy-Item -LiteralPath (Join-Path (Join-Path $StageRoot $entry.Key) "LOG.TXT") `
            -Destination (Join-Path $EvidenceDirectory ($entry.Key + ".audit"))
    }
    Write-Output "Runtime-verified: Scrapbook and HelloWorld completed independent audits while running concurrently."
} finally {
    foreach ($process in $processes) {
        if (-not $process.HasExited) {
            [void]$process.CloseMainWindow()
            if (-not $process.WaitForExit(5000)) {
                Stop-Process -Id $process.Id -Force
                $process.WaitForExit()
            }
        }
        $process.Dispose()
    }
}
