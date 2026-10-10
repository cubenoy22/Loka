#ifndef LOKA_TOOLBOX_OUT_OF_MEMORY_HPP
#define LOKA_TOOLBOX_OUT_OF_MEMORY_HPP

namespace loka { namespace toolbox {
/** At turn end, report and quit if a Toolbox request spent the reserve.
    One phase compare per event-loop turn; walks nothing. */
void QuitIfOutOfMemoryReserveSpent();
/** Reserve process memory once after InitDialogs for the fatal OOM dialog. */
void ArmOutOfMemoryReserve();
/** Release the reserve, report memory exhaustion if armed, then ExitToShell.
    Never returns on Classic; before arming or on reentry, exits immediately. */
#if defined(LOKA_RETRO68)
__attribute__((noreturn))
#endif
void QuitForOutOfMemory();
} }
#endif
