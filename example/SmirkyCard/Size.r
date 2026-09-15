#include "Processes.r"

/* Partition sized from SmirkyCard itself (2026-09-16, MAME Macintosh II
   and Macintosh Plus, System 7, MAIN.JS loaded, both cards visited): about
   1.1 MB of the 3 MiB partition in use ("About This Macintosh"), which is
   CODE 0.9 MB + DATA 0.1 MB + engine peak 0.29 MB less what stays purgeable.
   Budget: the per-engine QuickJS ceiling is 512 KiB (ScriptEngine.c) and a
   reload keeps two engines alive for one admission cycle, so the worst case
   is 0.8 MB + 2 x 512 KiB = 1.8 MB, inside the 2 MiB preferred size. At the
   1.5 MiB minimum startup always fits (0.8 MB + 512 KiB) and a reload whose
   candidate cannot get its heap is refused on the current card (QuickJS
   out-of-memory exception), not crashed. On a 4 MB Plus with a 1 MB System
   the preferred size is granted. Re-measure before any smaller claim. */
resource 'SIZE' (-1) {
    reserved, acceptSuspendResumeEvents, reserved, canBackground,
    doesActivateOnFGSwitch, backgroundAndForeground, dontGetFrontClicks,
    ignoreChildDiedEvents, is32BitCompatible, notHighLevelEventAware,
    onlyLocalHLEvents, notStationeryAware, dontUseTextEditServices,
    reserved, reserved, reserved,
    2 * 1024 * 1024, 1536 * 1024
};
