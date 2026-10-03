// SIZE partition override for the Retro68/Classic build.
// SimpleViewer decodes whole PICTs into the heap, so its need grows with the picture.
// Measured need (#1103, MAME maciix, 2026-10-03): 520.0K = peak live heap
// 494460 B + stack/A5/zone 38060 B, under the workload in
// tests/toolbox/measure-example-heaps.sh (rerun it after a size-relevant change).
// 68K: minimum = need x 1.1 rounded up to 32K. Preferred stays 1024K instead of
// need x 1.5 (832K): the 13.8 KB picture above is small, and larger ones
// raise the need until the open probe refuses them.
// Rez resource ordering: Retro68APPL.r's SIZE (-1) is Rezzed first; this one
// is appended after and overrides it.
#include "Processes.r"
#include "LokaClassicTarget.r"

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
	/* Carbon partitions unmeasured: keep the RetroCarbonAPPL.r defaults so
	   a Carbon build is flag- and size-identical to the template. */
	1024 * 1024,
	1024 * 1024
#elif LOKA_CLASSIC_68K
	1024 * 1024,	/* preferred */
	576 * 1024	/* minimum */
#else
	/* PPC partitions are unmeasured: keep the values they had before the
	   68K measurement (#1103). */
	1024 * 1024,	/* preferred */
	512 * 1024	/* minimum */
#endif
};
