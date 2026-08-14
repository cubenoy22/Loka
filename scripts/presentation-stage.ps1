# PowerShell twin of presentation-stage.sh. Keep the transaction states
# aligned while leaving platform-specific population and validation to callers.
function Install-LokaPresentationStageDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$StageRoot,

        [Parameter(Mandatory = $true)]
        [scriptblock]$Populate
    )

    $resolvedStageRoot = [System.IO.Path]::GetFullPath($StageRoot)
    $parent = Split-Path -Parent $resolvedStageRoot
    $stageName = Split-Path -Leaf $resolvedStageRoot
    if (-not $parent -or -not $stageName -or $resolvedStageRoot -eq [System.IO.Path]::GetPathRoot($resolvedStageRoot)) {
        throw "Refusing unsafe presentation stage path: $StageRoot"
    }

    $transactionId = "$PID-$([Guid]::NewGuid().ToString('N'))"
    $stagingRoot = Join-Path $parent ".$stageName.staging.$transactionId"
    $backupRoot = Join-Path $parent ".$stageName.previous.$transactionId"
    $oldStageMoved = $false
    $replacementCommitted = $false

    New-Item -ItemType Directory -Path $parent -Force | Out-Null
    New-Item -ItemType Directory -Path $stagingRoot | Out-Null
    try {
        & $Populate $stagingRoot
        if (-not (Test-Path -LiteralPath $stagingRoot -PathType Container)) {
            throw "Presentation populate function removed its staging directory."
        }

        if (Test-Path -LiteralPath $resolvedStageRoot) {
            Move-Item -LiteralPath $resolvedStageRoot -Destination $backupRoot
            $oldStageMoved = $true
        }

        try {
            Move-Item -LiteralPath $stagingRoot -Destination $resolvedStageRoot
            $replacementCommitted = $true
        } catch {
            $replacementFailure = $_
            if ($oldStageMoved) {
                try {
                    Move-Item -LiteralPath $backupRoot -Destination $resolvedStageRoot
                    $oldStageMoved = $false
                } catch {
                    throw (("Presentation replacement failed and the previous stage could not be restored: {0}; " +
                        "restore failure: {1}") -f $replacementFailure, $_)
                }
            }
            throw $replacementFailure
        }

        if ($oldStageMoved) {
            try {
                Remove-Item -LiteralPath $backupRoot -Recurse -Force
                $oldStageMoved = $false
            } catch {
                # A scanner or a running verifier can retain the retired copy.
                # The completed stage is already committed, so leave the
                # sibling for a later manual cleanup instead of undoing it.
                Write-Warning "The previous presentation stage could not be removed: $backupRoot"
            }
        }
    } finally {
        if (-not $replacementCommitted -and $oldStageMoved `
            -and -not (Test-Path -LiteralPath $resolvedStageRoot)) {
            Move-Item -LiteralPath $backupRoot -Destination $resolvedStageRoot
            $oldStageMoved = $false
        }
        if (Test-Path -LiteralPath $stagingRoot) {
            Remove-Item -LiteralPath $stagingRoot -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}
