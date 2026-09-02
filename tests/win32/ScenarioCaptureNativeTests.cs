using System;

public static class LokaScenarioCaptureNativeTests {
    private sealed class FakeDpiProcessApi : LokaScenarioDpiProcessApi {
        public bool perMonitorUnavailable;
        public int perMonitorResult;
        public bool perMonitorCurrent;
        public bool systemUnavailable;
        public bool systemResult;
        public bool systemCurrent;
        public int systemCalls;

        public int SetPerMonitorAware() {
            if (perMonitorUnavailable) throw new EntryPointNotFoundException();
            return perMonitorResult;
        }

        public bool IsPerMonitorAware() {
            return perMonitorCurrent;
        }

        public bool SetSystemAware() {
            ++systemCalls;
            if (systemUnavailable) throw new EntryPointNotFoundException();
            return systemResult;
        }

        public bool IsSystemAware() {
            return systemCurrent;
        }
    }

    private static void Require(bool condition, string message) {
        if (!condition) throw new InvalidOperationException(message);
    }

    public static void Run() {
        FakeDpiProcessApi perMonitor = new FakeDpiProcessApi();
        perMonitor.perMonitorResult = 0;
        Require(
            LokaScenarioNative.PrepareCaptureProcess(perMonitor)
                == LokaScenarioDpiSetupResult.ProcessCoordinatesArePhysical,
            "a successful Windows 8.1 per-monitor setup must provide physical coordinates");
        Require(perMonitor.systemCalls == 0,
            "an available per-monitor API must not fall through to a weaker system setup");

        FakeDpiProcessApi existingPerMonitor = new FakeDpiProcessApi();
        existingPerMonitor.perMonitorResult = unchecked((int)0x80070005);
        existingPerMonitor.perMonitorCurrent = true;
        Require(
            LokaScenarioNative.PrepareCaptureProcess(existingPerMonitor)
                == LokaScenarioDpiSetupResult.ProcessCoordinatesArePhysical,
            "an already per-monitor-aware process must remain usable");

        FakeDpiProcessApi refusedPerMonitor = new FakeDpiProcessApi();
        refusedPerMonitor.perMonitorResult = unchecked((int)0x80070005);
        Require(
            LokaScenarioNative.PrepareCaptureProcess(refusedPerMonitor)
                == LokaScenarioDpiSetupResult.NeedsThreadOverride,
            "a present but refused per-monitor API must not silently degrade to system awareness");
        Require(refusedPerMonitor.systemCalls == 0,
            "Windows 8.1 must not substitute system awareness for the target's per-monitor manifest");

        FakeDpiProcessApi vista = new FakeDpiProcessApi();
        vista.perMonitorUnavailable = true;
        vista.systemResult = true;
        Require(
            LokaScenarioNative.PrepareCaptureProcess(vista)
                == LokaScenarioDpiSetupResult.ProcessCoordinatesArePhysical,
            "Vista and Windows 7 must use the system-awareness capability");
        Require(vista.systemCalls == 1,
            "the system fallback must be attempted exactly once");

        FakeDpiProcessApi existingSystem = new FakeDpiProcessApi();
        existingSystem.perMonitorUnavailable = true;
        existingSystem.systemCurrent = true;
        Require(
            LokaScenarioNative.PrepareCaptureProcess(existingSystem)
                == LokaScenarioDpiSetupResult.ProcessCoordinatesArePhysical,
            "an already system-aware Vista or Windows 7 process must remain usable");

        FakeDpiProcessApi unavailable = new FakeDpiProcessApi();
        unavailable.perMonitorUnavailable = true;
        Require(
            LokaScenarioNative.PrepareCaptureProcess(unavailable)
                == LokaScenarioDpiSetupResult.NeedsThreadOverride,
            "a failed legacy setup must require a thread override or refuse capture");

        FakeDpiProcessApi preVista = new FakeDpiProcessApi();
        preVista.perMonitorUnavailable = true;
        preVista.systemUnavailable = true;
        Require(
            LokaScenarioNative.PrepareCaptureProcess(preVista)
                == LokaScenarioDpiSetupResult.ProcessCoordinatesArePhysical,
            "pre-Vista coordinates have no DPI virtualization seam");
    }
}
