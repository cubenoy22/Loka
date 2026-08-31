param(
  [Parameter(Mandatory = $true)]
  [string] $Application
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class LokaResizeProbeNative
{
    private delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool MoveWindow(
        IntPtr hwnd,
        int x,
        int y,
        int width,
        int height,
        bool repaint);

    [DllImport("user32.dll")]
    private static extern bool EnumChildWindows(
        IntPtr parent,
        EnumWindowsProc callback,
        IntPtr lParam);

    public static int CountChildWindows(IntPtr parent)
    {
        int count = 0;
        EnumChildWindows(parent, delegate(IntPtr hwnd, IntPtr unused) {
            ++count;
            return true;
        }, IntPtr.Zero);
        return count;
    }
}
"@

function Wait-MainWindow([System.Diagnostics.Process] $Process)
{
  $deadline = [DateTime]::UtcNow.AddSeconds(10)
  do
  {
    Start-Sleep -Milliseconds 50
    $Process.Refresh()
    if ($Process.HasExited)
    {
      throw "Process exited before creating a main window: $($Process.StartInfo.FileName)"
    }
    if ($Process.MainWindowHandle -ne [IntPtr]::Zero)
    {
      return $Process.MainWindowHandle
    }
  } while ([DateTime]::UtcNow -lt $deadline)
  throw "Timed out waiting for a main window: $($Process.StartInfo.FileName)"
}

function Get-LatencySummary([System.Collections.Generic.List[double]] $Samples)
{
  $sorted = $Samples.ToArray()
  [Array]::Sort($sorted)
  $sum = 0.0
  foreach ($sample in $sorted)
  {
    $sum += $sample
  }
  $medianIndex = [Math]::Floor(($sorted.Length - 1) * 0.50)
  $p95Index = [Math]::Floor(($sorted.Length - 1) * 0.95)
  [PSCustomObject]@{
    samples = $sorted.Length
    minimum_ms = [Math]::Round($sorted[0], 3)
    median_ms = [Math]::Round($sorted[$medianIndex], 3)
    p95_ms = [Math]::Round($sorted[$p95Index], 3)
    maximum_ms = [Math]::Round($sorted[$sorted.Length - 1], 3)
    mean_ms = [Math]::Round($sum / $sorted.Length, 3)
  }
}

function Measure-Moves(
  [IntPtr] $Window,
  [int] $FirstX,
  [int] $FirstY,
  [int] $FirstWidth,
  [int] $FirstHeight,
  [int] $SecondX,
  [int] $SecondY,
  [int] $SecondWidth,
  [int] $SecondHeight)
{
  $samples = [System.Collections.Generic.List[double]]::new()
  for ($index = 0; $index -lt 80; ++$index)
  {
    if (($index % 2) -eq 0)
    {
      $x = $FirstX
      $y = $FirstY
      $width = $FirstWidth
      $height = $FirstHeight
    }
    else
    {
      $x = $SecondX
      $y = $SecondY
      $width = $SecondWidth
      $height = $SecondHeight
    }

    $start = [System.Diagnostics.Stopwatch]::GetTimestamp()
    if (-not [LokaResizeProbeNative]::MoveWindow($Window, $x, $y, $width, $height, $true))
    {
      throw "MoveWindow failed with Win32 error $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"
    }
    $finish = [System.Diagnostics.Stopwatch]::GetTimestamp()
    $samples.Add(1000.0 * ($finish - $start) / [System.Diagnostics.Stopwatch]::Frequency)
    Start-Sleep -Milliseconds 8
  }
  Get-LatencySummary $samples
}

$resolvedApplication = (Resolve-Path $Application).Path
$process = Start-Process -FilePath $resolvedApplication -PassThru
try
{
  $window = Wait-MainWindow $process
  if (-not [LokaResizeProbeNative]::MoveWindow($window, 120, 120, 640, 480, $true))
  {
    throw "Initial MoveWindow failed"
  }
  Start-Sleep -Milliseconds 500

  [PSCustomObject]@{
    application = [IO.Path]::GetFileName($resolvedApplication)
    child_hwnds = [LokaResizeProbeNative]::CountChildWindows($window)
    position_only = Measure-Moves $window 120 120 640 480 180 120 640 480
    resize = Measure-Moves $window 120 120 640 480 120 120 1000 760
  } | ConvertTo-Json -Depth 4
}
finally
{
  if (-not $process.HasExited)
  {
    [void] $process.CloseMainWindow()
    if (-not $process.WaitForExit(2000))
    {
      $process.Kill()
      $process.WaitForExit()
    }
  }
  $process.Dispose()
}
