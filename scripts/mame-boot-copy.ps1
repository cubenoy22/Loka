function Resolve-FileIdentity([string]$Path) {
    $full = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
    $item = Get-Item -LiteralPath $full -Force -ErrorAction SilentlyContinue
    while ($item -and ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint)) {
        $target = $item.Target
        if (-not $target) { break }
        if (-not [System.IO.Path]::IsPathRooted($target)) { $target = Join-Path (Split-Path -Parent $full) $target }
        $full = [System.IO.Path]::GetFullPath($target)
        $item = Get-Item -LiteralPath $full -Force -ErrorAction SilentlyContinue
    }
    return [System.IO.Path]::GetFullPath($full).TrimEnd([char[]]"\/")
}

function Prepare-LokaBootCopy([string]$Template, [string]$BootDisk) {
    if (-not (Test-Path -LiteralPath $Template -PathType Leaf)) { throw "boot hard disk template not found: $Template" }
    New-Item -ItemType Directory -Path (Split-Path -Parent $BootDisk) -Force | Out-Null
    if ((Test-Path -LiteralPath $BootDisk) -and ((Resolve-FileIdentity $BootDisk) -ieq (Resolve-FileIdentity $Template))) { throw "MAME_BOOT_HDA resolves to the boot template itself: $BootDisk" }
    $templatePath = Resolve-FileIdentity $Template
    $templateSha = (Get-FileHash -LiteralPath $templatePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $source = "$BootDisk.source"
    $oldSource = @(if (Test-Path -LiteralPath $source -PathType Leaf) { @([System.IO.File]::ReadAllLines($source)) } else { @() })
    $oldTemplatePath = if ($oldSource.Count -eq 2 -and $oldSource[1] -ceq $templateSha) { try { Resolve-FileIdentity $oldSource[0] } catch { $null } }
    $previous = "$BootDisk.previous"
    if ((Test-Path -LiteralPath $previous) -or (Test-Path -LiteralPath "$source.previous") -or (Test-Path -LiteralPath $BootDisk -PathType Container) -or (Test-Path -LiteralPath $source -PathType Container)) { throw "boot copy: refresh failed (destination or recovery path needs attention)" }
    $reason = if (-not (Test-Path -LiteralPath $BootDisk -PathType Leaf)) { "copy missing" } elseif (-not (Test-Path -LiteralPath $source -PathType Leaf)) { "source missing" } elseif ($oldSource.Count -ne 2 -or $oldSource[1] -cne $templateSha -or $oldTemplatePath -ine $templatePath) { "template changed: $($oldSource | Select-Object -First 1) $([char]0x2192) $templatePath" }
    if ($reason) {
        $partial = "$BootDisk.partial"; $phase = "staging"
        try {
            [System.IO.File]::WriteAllText("$source.partial", "$templatePath`n$templateSha`n", (New-Object System.Text.UTF8Encoding($false)))
            Copy-Item -LiteralPath $templatePath -Destination $partial -Force
            if (Test-Path -LiteralPath $source -PathType Leaf) { Copy-Item -LiteralPath $source -Destination "$source.previous" -Force }
            if (Test-Path -LiteralPath $BootDisk) { Move-Item -LiteralPath $BootDisk -Destination $previous -Force }
            $phase = "backed-up"; Move-Item -LiteralPath $partial -Destination $BootDisk -Force
            $phase = "installed"; Move-Item -LiteralPath "$source.partial" -Destination $source -Force
            $phase = "published"; if (Test-Path -LiteralPath $previous) { Remove-Item -LiteralPath $previous -Force }; $phase = "complete"
        } catch {
            if (Test-Path -LiteralPath $previous) { Move-Item -LiteralPath $previous -Destination $BootDisk -Force } elseif ($phase -eq "installed" -or $phase -eq "published") { Remove-Item -LiteralPath $BootDisk -Force }
            if ($phase -eq "published") { if (Test-Path -LiteralPath "$source.previous" -PathType Leaf) { Move-Item -LiteralPath "$source.previous" -Destination $source -Force } else { Remove-Item -LiteralPath $source -Force } }
            Remove-Item -LiteralPath $partial, "$source.partial", "$source.previous" -Force -ErrorAction SilentlyContinue
            if ($phase -eq "staging") { Write-Host "boot copy: refresh failed (previous copy unchanged)" } else { Write-Host "boot copy: refresh failed (previous copy restored)" }
            throw
        }
        Remove-Item -LiteralPath "$source.previous" -Force -ErrorAction SilentlyContinue; Write-Host "boot copy: refreshed ($reason)"
    } else { Write-Host "boot copy: reused (same template)" }
    Set-ItemProperty -LiteralPath $BootDisk -Name IsReadOnly -Value $false
}
