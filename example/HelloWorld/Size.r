// SIZE partition override for the Retro68/Classic build (#135 follow-up).
// Post-diet HelloWorld runs comfortably in 384K min / 512K preferred; this
// replaces the toolchain's default 1024K/1024K template partition, which
// Rez resource ordering guarantees (Retro68APPL.r's SIZE (-1) is Rezzed
// first, this one is appended after and overrides it).
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
	/* Carbon partitions unmeasured: keep the RetroCarbonAPPL.r defaults so
	   a Carbon build is flag- and size-identical to the template. */
	1024 * 1024,
	1024 * 1024
#else
	512 * 1024,	/* preferred */
	384 * 1024	/* minimum */
#endif
};
