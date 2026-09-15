#include "Processes.r"

/* Partition sized from SmirkyCard itself (2026-09-16, MAME Macintosh II
   and Macintosh Plus, System 7, MAIN.JS loaded, both cards visited): about
   1.1 MB of the 3 MiB partition in use ("About This Macintosh"), which is
   CODE 0.9 MB + DATA 0.1 MB + engine peak 0.29 MB less what stays purgeable.
   A reload adds one more engine for at most one admission cycle (~0.12 MB).
   1.5 MiB minimum, 2 MiB preferred; on a 4 MB Plus with a 1 MB System the
   preferred size still leaves room. Re-measure before any smaller claim. */
resource 'SIZE' (-1) {
    reserved, acceptSuspendResumeEvents, reserved, canBackground,
    doesActivateOnFGSwitch, backgroundAndForeground, dontGetFrontClicks,
    ignoreChildDiedEvents, is32BitCompatible, notHighLevelEventAware,
    onlyLocalHLEvents, notStationeryAware, dontUseTextEditServices,
    reserved, reserved, reserved,
    2 * 1024 * 1024, 1536 * 1024
};
