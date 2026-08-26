// Shared SIZE partition for the loop presentation vehicles (#402).
//
// A loop vehicle runs its example's cells forever, so it is an endurance run
// by construction: every pass tears the composed tree down and rebuilds it,
// and MineSweeper's pass crosses a Section bank swap twice per New Game. #398
// records that the shipping 512K MineSweeper partition has no headroom left at
// that swap, so a loop build sitting on the shipping budget would die of a
// known ceiling instead of reporting what it found. The enlarged partition
// (PR #400's approach for the scenario vehicle) keeps a soak run measuring
// leaks rather than measuring the ceiling.
//
// One file rather than one per example: the reason is the reel, not the
// example, so the two vehicles must not be able to drift apart.
#include "Processes.r"

resource 'SIZE' (-1) {
	reserved,
#if TARGET_API_MAC_CARBON
	acceptSuspendResumeEvents,
	reserved,
	canBackground,
	doesActivateOnFGSwitch,
#else
	acceptSuspendResumeEvents,
	reserved,
	canBackground,
	doesActivateOnFGSwitch,
#endif
	backgroundAndForeground,
	dontGetFrontClicks,
	ignoreChildDiedEvents,
	is32BitCompatible,
#if TARGET_API_MAC_CARBON
	isHighLevelEventAware,
#else
	notHighLevelEventAware,
#endif
	onlyLocalHLEvents,
	notStationeryAware,
	dontUseTextEditServices,
	reserved,
	reserved,
	reserved,
#if TARGET_API_MAC_CARBON
	1024 * 1024,
	1024 * 1024
#else
	640 * 1024,	/* preferred */
	512 * 1024	/* minimum */
#endif
};
