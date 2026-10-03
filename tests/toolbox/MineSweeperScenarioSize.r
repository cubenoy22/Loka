// The scenario vehicle carries the production board plus audit and actuation
// vocabulary. Keep its partition separate from the shipping example's measured
// budget (#1103): a second registered tour crossed the old 384K/512K partition
// during the existing two-New-Game recomposition sequence.
#include "Processes.r"

resource 'SIZE' (-1) {
	reserved,
#if TARGET_API_MAC_CARBON
	acceptSuspendResumeEvents,
	reserved,
	canBackground,
	doesActivateOnFGSwitch,
#else
	ignoreSuspendResumeEvents,
	reserved,
	cannotBackground,
	needsActivateOnFGSwitch,
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
