param(
    [Parameter(Position = 0, Mandatory = $true)]
    [string]$Example,
    [Parameter(Position = 1, Mandatory = $true)]
    [string]$Scenario,
    [Alias("update-golden")]
    [switch]$UpdateGolden
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2

$ProjectDirectory = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Registry = Join-Path $ProjectDirectory "tests/scenarios/scenarios.txt"
$FixtureRegistry = Join-Path $ProjectDirectory "tests/scenarios/scrapbook-package-fixtures.txt"
$ExpectedAudit = Join-Path $ProjectDirectory "tests/scenarios/expected/$Example/$Scenario.audit"
$BuiltExecutable = Join-Path $ProjectDirectory `
    "build/win32/Debug/example/ScrapbookUI/LokaScrapbookScenarioWin32.exe"
$Lrpc = Join-Path $ProjectDirectory "build/host/lrpc/lrpc.exe"
$MultiConfigLrpc = Join-Path $ProjectDirectory "build/host/lrpc/Debug/lrpc.exe"
$SourceAssets = Join-Path $ProjectDirectory "example/ScrapbookUI/assets/ASSETS-modern.LRP"
$Work = Join-Path $ProjectDirectory "build/win32-scenario/$Example/$Scenario"
$Stage = Join-Path $Work "stage"
$Settle = Join-Path $Work "settle"
$GoldenDirectory = Join-Path $ProjectDirectory "build/win32-scenario/golden/$Example"
$Golden = Join-Path $GoldenDirectory "$Scenario.png"
$GoldenProfile = Join-Path $GoldenDirectory "$Scenario.profile"
$PngTool = Join-Path $ProjectDirectory "tests/scenarios/pngtool.py"
$RunnerLog = Join-Path $Work "runner.log"
$ChildProcess = $null
$ChildWindow = [IntPtr]::Zero
$ChildStdoutTask = $null
$ChildStderrTask = $null
$Python = $null
$UseWslPython = $false
$PngExitCode = 0
$ExitCode = 0

function Fail-Stage([string]$StageName, [string]$Message) {
    throw "$StageName stage failed: $Message"
}

function Write-Runner([string]$Message) {
    Write-Output $Message
    if (Test-Path -LiteralPath $Work) {
        Add-Content -LiteralPath $RunnerLog -Value $Message -Encoding UTF8
    }
}

function Test-FilesEqual([string]$First, [string]$Second) {
    $firstStream = [System.IO.File]::OpenRead($First)
    try {
        $secondStream = [System.IO.File]::OpenRead($Second)
        try {
            if ($firstStream.Length -ne $secondStream.Length) {
                return $false
            }
            $firstBuffer = New-Object byte[] 65536
            $secondBuffer = New-Object byte[] 65536
            while ($true) {
                $firstCount = $firstStream.Read($firstBuffer, 0, $firstBuffer.Length)
                $secondCount = $secondStream.Read($secondBuffer, 0, $secondBuffer.Length)
                if ($firstCount -ne $secondCount) {
                    return $false
                }
                if ($firstCount -eq 0) {
                    return $true
                }
                for ($index = 0; $index -lt $firstCount; ++$index) {
                    if ($firstBuffer[$index] -ne $secondBuffer[$index]) {
                        return $false
                    }
                }
            }
        } finally {
            $secondStream.Dispose()
        }
    } finally {
        $firstStream.Dispose()
    }
}

function Read-CaptureBounds([string]$Path) {
    $values = @{}
    foreach ($line in [System.IO.File]::ReadAllLines($Path)) {
        if ($line -notmatch '^([a-z_]+)=(-?[0-9]+)$') {
            Fail-Stage "crop" "invalid capture bounds line '$line'"
        }
        if ($values.ContainsKey($Matches[1])) {
            Fail-Stage "crop" "duplicate capture bounds key '$($Matches[1])'"
        }
        $values[$Matches[1]] = [long]$Matches[2]
    }
    $expectedKeys = @("bounds_version", "left", "top", "right", "bottom")
    if ($values.Count -ne $expectedKeys.Count) {
        Fail-Stage "crop" "capture bounds contain unexpected or missing keys"
    }
    foreach ($key in $expectedKeys) {
        if (-not $values.ContainsKey($key)) {
            Fail-Stage "crop" "capture bounds omit '$key'"
        }
    }
    if ($values["bounds_version"] -ne 1 `
        -or $values["left"] -lt 0 -or $values["top"] -lt 0 `
        -or $values["right"] -le $values["left"] `
        -or $values["bottom"] -le $values["top"]) {
        Fail-Stage "crop" "capture bounds are invalid"
    }
    return $values
}

function Get-CorruptBag([string]$ScenarioName) {
    $result = $null
    foreach ($line in [System.IO.File]::ReadAllLines($FixtureRegistry)) {
        if ($line -notmatch '^([a-z0-9][a-z0-9-]*) corrupt-bag=([0-9]+)$') {
            Fail-Stage "stage" "invalid Scrapbook fixture registry line '$line'"
        }
        if ($Matches[1] -eq $ScenarioName) {
            if ($null -ne $result) {
                Fail-Stage "stage" "duplicate fixture mapping for '$ScenarioName'"
            }
            $result = [int]$Matches[2]
        }
    }
    return $result
}

function Resolve-Python {
    if ($env:PYTHON3) {
        $script:Python = $env:PYTHON3
        return
    }
    $python = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($python -and $python.Source -notmatch '\\Microsoft\\WindowsApps\\python\.exe$') {
        $script:Python = $python.Source
        return
    }
    $wsl = Get-Command wsl.exe -ErrorAction SilentlyContinue
    if ($wsl) {
        $script:UseWslPython = $true
        return
    }
    Fail-Stage "build" "a real python.exe or WSL python3 is required for exact PNG crop/compare"
}

function Convert-ToWslPath([string]$Path) {
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    if ($fullPath -notmatch '^([A-Za-z]):\\(.*)$') {
        Fail-Stage "crop" "WSL Python requires a drive-letter path, got '$fullPath'"
    }
    $drive = $Matches[1].ToLowerInvariant()
    $tail = $Matches[2].Replace('\', '/')
    return "/mnt/$drive/$tail"
}

function Invoke-PngCrop([string]$InputPath, [long]$Left, [long]$Top, [long]$Right, [long]$Bottom, [string]$OutputPath) {
    if ($UseWslPython) {
        & wsl.exe python3 (Convert-ToWslPath $PngTool) crop `
            (Convert-ToWslPath $InputPath) $Left $Top $Right $Bottom (Convert-ToWslPath $OutputPath)
    } else {
        & $Python $PngTool crop $InputPath $Left $Top $Right $Bottom $OutputPath
    }
    $script:PngExitCode = $LASTEXITCODE
}

function Invoke-PngCompare([string]$First, [string]$Second) {
    if ($UseWslPython) {
        & wsl.exe python3 (Convert-ToWslPath $PngTool) compare `
            (Convert-ToWslPath $First) (Convert-ToWslPath $Second)
    } else {
        & $Python $PngTool compare $First $Second
    }
    $script:PngExitCode = $LASTEXITCODE
}

function Invoke-PngDiff([string]$Expected, [string]$Actual, [string]$OutputPath) {
    if ($UseWslPython) {
        & wsl.exe python3 (Convert-ToWslPath $PngTool) diff `
            (Convert-ToWslPath $Expected) (Convert-ToWslPath $Actual) (Convert-ToWslPath $OutputPath)
    } else {
        & $Python $PngTool diff $Expected $Actual $OutputPath
    }
    $script:PngExitCode = $LASTEXITCODE
}

$nativeSource = @'
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class LokaScenarioNative {
    [StructLayout(LayoutKind.Sequential)]
    private struct RECT { public int Left, Top, Right, Bottom; }

    private delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr parameter);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);
    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")]
    private static extern IntPtr GetWindow(IntPtr hwnd, uint command);
    [DllImport("user32.dll")]
    private static extern bool GetWindowRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")]
    private static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
    [DllImport("user32.dll", EntryPoint = "PostMessageW")]
    private static extern bool PostMessageW(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);

    public static IntPtr[] VisibleTopLevelWindows(uint processId) {
        List<IntPtr> result = new List<IntPtr>();
        EnumWindows(delegate(IntPtr hwnd, IntPtr parameter) {
            uint owner;
            GetWindowThreadProcessId(hwnd, out owner);
            if (owner == processId && IsWindowVisible(hwnd) && GetWindow(hwnd, 4) == IntPtr.Zero) {
                result.Add(hwnd);
            }
            return true;
        }, IntPtr.Zero);
        return result.ToArray();
    }

    public static bool CaptureWindow(IntPtr hwnd, string path) {
        RECT rect;
        if (!GetWindowRect(hwnd, out rect)) return false;
        int width = rect.Right - rect.Left;
        int height = rect.Bottom - rect.Top;
        if (width <= 0 || height <= 0) return false;
        using (Bitmap bitmap = new Bitmap(width, height, PixelFormat.Format32bppArgb))
        using (Graphics graphics = Graphics.FromImage(bitmap)) {
            IntPtr hdc = graphics.GetHdc();
            bool captured;
            try {
                captured = PrintWindow(hwnd, hdc, 2);
            } finally {
                graphics.ReleaseHdc(hdc);
            }
            if (!captured) return false;
            Color first = bitmap.GetPixel(0, 0);
            bool varied = false;
            for (int y = 0; y < height && !varied; ++y) {
                for (int x = 0; x < width; ++x) {
                    if (bitmap.GetPixel(x, y).ToArgb() != first.ToArgb()) {
                        varied = true;
                        break;
                    }
                }
            }
            if (!varied) return false;
            bitmap.Save(path, ImageFormat.Png);
        }
        return true;
    }

    public static bool CloseWindow(IntPtr hwnd) {
        return hwnd != IntPtr.Zero && PostMessageW(hwnd, 0x0010, IntPtr.Zero, IntPtr.Zero);
    }

}
'@

try {
    if ($Example -notmatch '^[a-z0-9][a-z0-9-]*$' `
        -or $Scenario -notmatch '^[a-z0-9][a-z0-9-]*$') {
        Fail-Stage "arguments" "usage: run-scenario.ps1 <example> <scenario> [--update-golden]"
    }
    if (-not (Test-Path -LiteralPath $Registry -PathType Leaf) `
        -or -not ([System.IO.File]::ReadAllLines($Registry) -contains "$Example $Scenario")) {
        Fail-Stage "arguments" "scenario '$Example/$Scenario' is not registered"
    }
    if ($Example -ne "scrapbook") {
        Fail-Stage "arguments" "the Win32 runner currently supports only ScrapbookUI"
    }
    if (-not (Test-Path -LiteralPath $BuiltExecutable -PathType Leaf)) {
        Fail-Stage "build" "missing $BuiltExecutable; enter vcvarsall.bat arm64, run cmake --preset win32-debug, then cmake --build --preset win32-tests"
    }
    if (-not (Test-Path -LiteralPath $Lrpc -PathType Leaf)) {
        if (Test-Path -LiteralPath $MultiConfigLrpc -PathType Leaf) {
            $Lrpc = $MultiConfigLrpc
        } else {
            Fail-Stage "build" "missing native lrpc.exe under build/host/lrpc; run cmake -S tools/lrpc -B build/host/lrpc and cmake --build build/host/lrpc"
        }
    }
    if (-not (Test-Path -LiteralPath $SourceAssets -PathType Leaf)) {
        Fail-Stage "stage" "missing $SourceAssets"
    }
    if (-not (Test-Path -LiteralPath $ExpectedAudit -PathType Leaf)) {
        Fail-Stage "verdict" "missing tracked audit $ExpectedAudit"
    }
    Resolve-Python

    if (Test-Path -LiteralPath $Work) {
        Remove-Item -LiteralPath $Work -Recurse -Force
    }
    New-Item -ItemType Directory -Path $Stage, $Settle -Force | Out-Null
    [System.IO.File]::WriteAllText($RunnerLog, "", (New-Object System.Text.UTF8Encoding($false)))
    Write-Runner "stage: build artifact $BuiltExecutable"
    Write-Runner "stage: native directory (no development disk)"

    $StagedExecutable = Join-Path $Stage "LokaScrapbookScenarioWin32.exe"
    $StagedAssets = Join-Path $Stage "ASSETS.LRP"
    Copy-Item -LiteralPath $BuiltExecutable -Destination $StagedExecutable
    $stageArguments = @("stage", $SourceAssets, "-o", $StagedAssets)
    $corruptBag = Get-CorruptBag $Scenario
    if ($null -ne $corruptBag) {
        $stageArguments += @("--corrupt-bag", [string]$corruptBag)
    }
    & $Lrpc @stageArguments *>> $RunnerLog
    if ($LASTEXITCODE -ne 0) {
        Fail-Stage "stage" "lrpc refused the staged package; see $RunnerLog"
    }
    [System.IO.File]::WriteAllText(
        (Join-Path $Stage "LokaTest.cfg"),
        "scenario $Scenario`ncapture_dir .`nlinger_seconds 120`n",
        (New-Object System.Text.UTF8Encoding($false)))

    Add-Type -AssemblyName System.Drawing
    Add-Type -TypeDefinition $nativeSource -ReferencedAssemblies System.Drawing

    Write-Runner "stage: launch"
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $StagedExecutable
    $startInfo.WorkingDirectory = $Stage
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.CreateNoWindow = $false
    $ChildProcess = New-Object System.Diagnostics.Process
    $ChildProcess.StartInfo = $startInfo
    if (-not $ChildProcess.Start()) {
        Fail-Stage "launch" "could not start $StagedExecutable"
    }
    $ChildStdoutTask = $ChildProcess.StandardOutput.ReadToEndAsync()
    $ChildStderrTask = $ChildProcess.StandardError.ReadToEndAsync()
    [System.IO.File]::WriteAllText((Join-Path $Work "child.pid"), [string]$ChildProcess.Id)

    $Completion = Join-Path $Stage "LokaTestsWin32.complete"
    $deadline = [DateTime]::UtcNow.AddSeconds(120)
    while (-not (Test-Path -LiteralPath $Completion -PathType Leaf)) {
        $ChildProcess.Refresh()
        if ($ChildProcess.HasExited) {
            Fail-Stage "drive" "scenario process exited $($ChildProcess.ExitCode) before completion; see child.stderr.log"
        }
        if ([DateTime]::UtcNow -ge $deadline) {
            Fail-Stage "drive" "timed out waiting for the completion marker"
        }
        Start-Sleep -Milliseconds 100
    }
    if ([System.IO.File]::ReadAllText($Completion).Trim() -ne "audit-ready") {
        Fail-Stage "completion" "atomic completion marker is invalid"
    }

    $windows = [LokaScenarioNative]::VisibleTopLevelWindows([uint32]$ChildProcess.Id)
    if ($windows.Count -ne 1) {
        Fail-Stage "capture" "expected one visible top-level HWND for PID $($ChildProcess.Id), found $($windows.Count)"
    }
    $ChildWindow = $windows[0]
    Write-Runner ("stage: capture PID={0} HWND=0x{1:X}" -f $ChildProcess.Id, $ChildWindow.ToInt64())

    $previousHash = $null
    $acceptedFrame = $null
    $settleDeadline = [DateTime]::UtcNow.AddSeconds(30)
    for ($frame = 1; [DateTime]::UtcNow -lt $settleDeadline; ++$frame) {
        $framePath = Join-Path $Settle ("frame-{0:D3}.png" -f $frame)
        if (-not [LokaScenarioNative]::CaptureWindow($ChildWindow, $framePath)) {
            Fail-Stage "capture" "PrintWindow(PW_RENDERFULLCONTENT) refused frame $frame"
        }
        $hash = (Get-FileHash -LiteralPath $framePath -Algorithm SHA256).Hash.ToLowerInvariant()
        Write-Runner "settle frame $frame sha256=$hash"
        if ($null -ne $previousHash -and $hash -eq $previousHash) {
            $acceptedFrame = $framePath
            break
        }
        $previousHash = $hash
        Start-Sleep -Milliseconds 250
    }
    if ($null -eq $acceptedFrame) {
        Fail-Stage "settle" "no two consecutive screenshots had identical hashes"
    }
    $WindowActual = Join-Path $Work "window-actual.png"
    Copy-Item -LiteralPath $acceptedFrame -Destination $WindowActual

    if (-not [LokaScenarioNative]::CloseWindow($ChildWindow)) {
        Fail-Stage "shutdown" "WM_CLOSE could not be posted to the owned HWND"
    }
    if (-not $ChildProcess.WaitForExit(10000)) {
        Fail-Stage "shutdown" "scenario process did not exit after WM_CLOSE"
    }
    # Complete the redirected stdout/stderr drains before observing ExitCode;
    # the timed overload alone can leave the PowerShell Process wrapper's
    # terminal properties unmaterialized even after the native handle signaled.
    $ChildProcess.WaitForExit()
    $ChildProcess.Refresh()
    $childExitCode = $ChildProcess.ExitCode
    [System.IO.File]::WriteAllText((Join-Path $Work "child.stdout.log"), $ChildStdoutTask.Result)
    [System.IO.File]::WriteAllText((Join-Path $Work "child.stderr.log"), $ChildStderrTask.Result)
    [System.IO.File]::WriteAllText((Join-Path $Work "child-exit.txt"), [string]$childExitCode)
    if ($childExitCode -ne 0) {
        Fail-Stage "shutdown" "scenario process exited $childExitCode"
    }

    $StageAudit = Join-Path $Stage "actual.audit"
    $ActualAudit = Join-Path $Work "actual.audit"
    if (-not (Test-Path -LiteralPath $StageAudit -PathType Leaf)) {
        Fail-Stage "extract" "scenario process did not write actual.audit"
    }
    Copy-Item -LiteralPath $StageAudit -Destination $ActualAudit
    if (-not (Test-FilesEqual $ExpectedAudit $ActualAudit)) {
        Fail-Stage "verdict" "actual.audit differs byte-for-byte from $ExpectedAudit"
    }
    $expectedHash = (Get-FileHash -LiteralPath $ExpectedAudit -Algorithm SHA256).Hash.ToLowerInvariant()
    $actualHash = (Get-FileHash -LiteralPath $ActualAudit -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Runner "audit verdict: byte-identical expected_sha256=$expectedHash actual_sha256=$actualHash"

    $StageBounds = Join-Path $Stage "capture.bounds"
    $StageProfile = Join-Path $Stage "actual.profile"
    if (-not (Test-Path -LiteralPath $StageBounds -PathType Leaf) `
        -or -not (Test-Path -LiteralPath $StageProfile -PathType Leaf)) {
        Fail-Stage "artifacts" "scenario process omitted capture.bounds or actual.profile"
    }
    $Bounds = Read-CaptureBounds $StageBounds
    $Actual = Join-Path $Work "actual.png"
    Invoke-PngCrop $WindowActual `
        $Bounds["left"] $Bounds["top"] $Bounds["right"] $Bounds["bottom"] $Actual *>> $RunnerLog
    if ($PngExitCode -ne 0) {
        Fail-Stage "crop" "content bounds do not fit the captured window; see $RunnerLog"
    }
    $ActualProfile = Join-Path $Work "actual.profile"
    Copy-Item -LiteralPath $StageProfile -Destination $ActualProfile

    if ($UpdateGolden) {
        New-Item -ItemType Directory -Path $GoldenDirectory -Force | Out-Null
        Copy-Item -LiteralPath $Actual -Destination $Golden
        Copy-Item -LiteralPath $ActualProfile -Destination $GoldenProfile
        Write-Runner "Updated Win32 rig golden: $Golden"
        Write-Runner "Reminder: attach visual evidence to the PR; the rig-local golden is untracked."
        $ExitCode = 0
    } else {
        if (-not (Test-Path -LiteralPath $Golden -PathType Leaf) `
            -or -not (Test-Path -LiteralPath $GoldenProfile -PathType Leaf)) {
            Fail-Stage "golden" "missing rig-local golden/profile; rerun with --update-golden"
        }
        $WorkGolden = Join-Path $Work "golden.png"
        Copy-Item -LiteralPath $Golden -Destination $WorkGolden
        if (-not (Test-FilesEqual $ActualProfile $GoldenProfile)) {
            Fail-Stage "profile" "rig profile differs from $GoldenProfile"
        }
        Invoke-PngCompare $Actual $WorkGolden *>> $RunnerLog
        if ($PngExitCode -ne 0) {
            $Diff = Join-Path $Work "diff.png"
            Invoke-PngDiff $WorkGolden $Actual $Diff *>> $RunnerLog
            if (-not (Test-Path -LiteralPath $Diff -PathType Leaf)) {
                Fail-Stage "golden" "pixel mismatch was detected but diff.png could not be written"
            }
            Fail-Stage "golden" "actual pixels differ from $Golden; see actual.png, golden.png, and diff.png"
        }
        [System.IO.File]::WriteAllText((Join-Path $Work "verified"), "runner-verified`n")
        Write-Runner "Scenario passed: $Example/$Scenario"
        $ExitCode = 0
    }
} catch {
    $ExitCode = 1
    [Console]::Error.WriteLine($_.Exception.Message)
    [Console]::Error.WriteLine("Work directory left for inspection: $Work")
} finally {
    if ($null -ne $ChildProcess) {
        $ChildProcess.Refresh()
        if (-not $ChildProcess.HasExited) {
            if ($ChildWindow -ne [IntPtr]::Zero) {
                [void][LokaScenarioNative]::CloseWindow($ChildWindow)
                [void]$ChildProcess.WaitForExit(3000)
                $ChildProcess.Refresh()
            }
            if (-not $ChildProcess.HasExited) {
                Stop-Process -Id $ChildProcess.Id -Force -ErrorAction SilentlyContinue
                [void]$ChildProcess.WaitForExit(3000)
            }
        }
        if ((Test-Path -LiteralPath $Work) -and -not (Test-Path -LiteralPath (Join-Path $Work "child-exit.txt"))) {
            $childResult = if ($ChildProcess.HasExited) {
                [string]$ChildProcess.ExitCode
            } else {
                "forced-timeout"
            }
            [System.IO.File]::WriteAllText((Join-Path $Work "child-exit.txt"), $childResult)
        }
        if ($null -ne $ChildStdoutTask -and $ChildStdoutTask.IsCompleted) {
            [System.IO.File]::WriteAllText((Join-Path $Work "child.stdout.log"), $ChildStdoutTask.Result)
        }
        if ($null -ne $ChildStderrTask -and $ChildStderrTask.IsCompleted) {
            [System.IO.File]::WriteAllText((Join-Path $Work "child.stderr.log"), $ChildStderrTask.Result)
        }
    }
}

exit $ExitCode
