using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public enum LokaScenarioCaptureResult {
    PhysicalBoundsUnavailable,
    Refused,
    Uniform,
    Captured
}

public enum LokaScenarioDpiSetupResult {
    NeedsThreadOverride,
    ProcessCoordinatesArePhysical
}

internal interface LokaScenarioDpiProcessApi {
    int SetPerMonitorAware();
    bool IsPerMonitorAware();
    bool SetSystemAware();
    bool IsSystemAware();
}

public static class LokaScenarioNative {
    [StructLayout(LayoutKind.Sequential)]
    private struct RECT { public int Left, Top, Right, Bottom; }

    private delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr parameter);

    private sealed class NativeDpiProcessApi : LokaScenarioDpiProcessApi {
        [DllImport("shcore.dll")]
        private static extern int SetProcessDpiAwareness(int awareness);
        [DllImport("shcore.dll")]
        private static extern int GetProcessDpiAwareness(IntPtr process, out int awareness);
        [DllImport("user32.dll")]
        private static extern bool SetProcessDPIAware();
        [DllImport("user32.dll")]
        private static extern bool IsProcessDPIAware();

        public int SetPerMonitorAware() {
            return SetProcessDpiAwareness(2);
        }

        public bool IsPerMonitorAware() {
            int awareness;
            return GetProcessDpiAwareness(IntPtr.Zero, out awareness) >= 0 && awareness == 2;
        }

        public bool SetSystemAware() {
            return SetProcessDPIAware();
        }

        public bool IsSystemAware() {
            return IsProcessDPIAware();
        }
    }

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
    [DllImport("user32.dll")]
    private static extern IntPtr SetThreadDpiAwarenessContext(IntPtr dpiContext);
    [DllImport("user32.dll", EntryPoint = "PostMessageW")]
    private static extern bool PostMessageW(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);

    public static LokaScenarioDpiSetupResult PrepareCaptureProcess() {
        return PrepareCaptureProcess(new NativeDpiProcessApi());
    }

    internal static LokaScenarioDpiSetupResult PrepareCaptureProcess(LokaScenarioDpiProcessApi api) {
        if (api == null) return LokaScenarioDpiSetupResult.NeedsThreadOverride;
        try {
            int result = api.SetPerMonitorAware();
            return result >= 0 || api.IsPerMonitorAware()
                ? LokaScenarioDpiSetupResult.ProcessCoordinatesArePhysical
                : LokaScenarioDpiSetupResult.NeedsThreadOverride;
        } catch (DllNotFoundException) {
            // Windows before 8.1 has no Shcore process-awareness API.
        } catch (EntryPointNotFoundException) {
            // Windows before 8.1 has no Shcore process-awareness API.
        }

        try {
            return api.SetSystemAware() || api.IsSystemAware()
                ? LokaScenarioDpiSetupResult.ProcessCoordinatesArePhysical
                : LokaScenarioDpiSetupResult.NeedsThreadOverride;
        } catch (EntryPointNotFoundException) {
            // Before Vista there is no DPI virtualization to opt out of.
            return LokaScenarioDpiSetupResult.ProcessCoordinatesArePhysical;
        }
    }

    private static bool UsePhysicalCoordinates(LokaScenarioDpiSetupResult processSetup) {
        try {
            // V2 is available from Windows 10 1703. Version 1607 exposes this
            // API but accepts only the Per-Monitor V1 context.
            IntPtr previousContext = SetThreadDpiAwarenessContext(new IntPtr(-4));
            if (previousContext != IntPtr.Zero) return true;
            if (SetThreadDpiAwarenessContext(new IntPtr(-3)) != IntPtr.Zero) return true;
        } catch (EntryPointNotFoundException) {
            // Vista through Windows 8.1 rely on the pre-launch process setup.
        }
        return processSetup == LokaScenarioDpiSetupResult.ProcessCoordinatesArePhysical;
    }

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

    public static LokaScenarioCaptureResult CaptureWindow(
        IntPtr hwnd,
        string path,
        LokaScenarioDpiSetupResult processSetup) {
        // The target publishes physical-pixel crop bounds. Refuse rather than
        // compare a known-virtualized rectangle against a physical rendering.
        if (!UsePhysicalCoordinates(processSetup)) {
            return LokaScenarioCaptureResult.PhysicalBoundsUnavailable;
        }

        RECT rect;
        if (!GetWindowRect(hwnd, out rect)) return LokaScenarioCaptureResult.Refused;
        int width = rect.Right - rect.Left;
        int height = rect.Bottom - rect.Top;
        if (width <= 0 || height <= 0) return LokaScenarioCaptureResult.Refused;
        using (Bitmap bitmap = new Bitmap(width, height, PixelFormat.Format32bppArgb))
        using (Graphics graphics = Graphics.FromImage(bitmap)) {
            IntPtr hdc = graphics.GetHdc();
            bool captured;
            try {
                captured = PrintWindow(hwnd, hdc, 2);
            } finally {
                graphics.ReleaseHdc(hdc);
            }
            if (!captured) return LokaScenarioCaptureResult.Refused;
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
            if (!varied) return LokaScenarioCaptureResult.Uniform;
            bitmap.Save(path, ImageFormat.Png);
        }
        return LokaScenarioCaptureResult.Captured;
    }

    public static bool CloseWindow(IntPtr hwnd) {
        return hwnd != IntPtr.Zero && PostMessageW(hwnd, 0x0010, IntPtr.Zero, IntPtr.Zero);
    }
}
