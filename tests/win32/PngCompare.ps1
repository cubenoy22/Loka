function Invoke-PngCompare(
    [string]$First,
    [string]$Second,
    [long]$MaxDiffPx,
    [long]$MaxDiffColumns
) {
    $script:PngCompareResult = $null
    if ($UseWslPython) {
        $compareOutput = & wsl.exe python3 (Convert-ToWslPath $PngTool) compare `
            --max-diff-px $MaxDiffPx --max-diff-columns $MaxDiffColumns `
            (Convert-ToWslPath $First) (Convert-ToWslPath $Second) 2>&1
    } else {
        $compareOutput = & $Python $PngTool compare `
            --max-diff-px $MaxDiffPx --max-diff-columns $MaxDiffColumns `
            $First $Second 2>&1
    }
    $script:PngExitCode = $LASTEXITCODE
    $differenceCount = $null
    $differenceColumnCount = $null
    foreach ($line in @($compareOutput)) {
        Write-Output $line
        if ([string]$line -match 'differing pixels: ([0-9]+); differing columns: ([0-9]+); max-diff-px: ([0-9]+); max-diff-columns: ([0-9]+)') {
            $reportedPixelTolerance = [long]$Matches[3]
            $reportedColumnTolerance = [long]$Matches[4]
            if ($reportedPixelTolerance -ne $MaxDiffPx) {
                Fail-Stage "golden" "pngtool reported pixel tolerance $reportedPixelTolerance, expected $MaxDiffPx"
            }
            if ($reportedColumnTolerance -ne $MaxDiffColumns) {
                Fail-Stage "golden" "pngtool reported column tolerance $reportedColumnTolerance, expected $MaxDiffColumns"
            }
            $differenceCount = [long]$Matches[1]
            $differenceColumnCount = [long]$Matches[2]
        }
    }
    if ($null -ne $differenceCount -and $null -ne $differenceColumnCount) {
        $script:PngCompareResult = @{
            DifferenceCount = $differenceCount
            DifferenceColumnCount = $differenceColumnCount
        }
    }
}
