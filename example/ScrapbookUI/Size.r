// SIZE partition override for the Retro68/Classic build.
// ScrapbookUI keeps one LRPK bag and one decoded PICT view resident.
// Measured need (#1103, MAME maciix, 2026-10-03): 363.9K = peak live heap
// 339596 B + stack/A5/zone 32988 B, under the workload in
// tests/toolbox/measure-example-heaps.sh (rerun it after a size-relevant change).
// minimum = need x 1.1 rounded up to 32K, preferred = need x 1.5 rounded up to 64K.
// Rez resource ordering: Retro68APPL.r's SIZE (-1) is Rezzed first; this one
// is appended after and overrides it.
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
	576 * 1024,	/* preferred */
	416 * 1024	/* minimum */
#endif
};
