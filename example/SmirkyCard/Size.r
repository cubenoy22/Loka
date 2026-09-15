#include "Processes.r"

/* Partition for an 8 MB Classic machine, from the 68K engine probe
   (2026-09-15): CODE 0.77 MB + DATA 0.10 MB + engine peak 0.29 MB + the
   Toolbox UI; 2 MiB minimum, 3 MiB preferred. Re-measure inside SmirkyCard
   before any smaller claim. */
resource 'SIZE' (-1) {
    reserved, acceptSuspendResumeEvents, reserved, canBackground,
    doesActivateOnFGSwitch, backgroundAndForeground, dontGetFrontClicks,
    ignoreChildDiedEvents, is32BitCompatible, notHighLevelEventAware,
    onlyLocalHLEvents, notStationeryAware, dontUseTextEditServices,
    reserved, reserved, reserved,
    3 * 1024 * 1024, 2 * 1024 * 1024
};
