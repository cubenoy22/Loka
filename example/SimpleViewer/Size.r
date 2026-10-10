// SIZE partition override for the Retro68/Classic build.
// SimpleViewer decodes whole PICTs into the heap, so its need grows with the picture.
// Measured need (after #1107, MAME maciix, 2026-10-10): 537.1K = peak live heap
// 511916 B + stack/A5/zone 38072 B, under the workload in
// tests/toolbox/measure-example-heaps.sh (rerun it after a size-relevant change).
// 68K: minimum = need x 1.1 rounded up to 32K. Preferred stays 1024K instead of
// need x 1.5 (832K): the 13.8 KB picture above is small, and larger ones
// raise the need until the read refuses them (READ_ALLOCATION_REFUSED).
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
	608 * 1024	/* minimum */
#else
	/* PPC, measured on MAME pmac6100 / Mac OS 8.1 (2026-10-10, #1109):
	   need 112K with virtual memory off, where the Process Manager adds
	   the code fragment to this partition; with it on, the heap peak
	   without the code section is 212K. Both fit, so the values stay. */
	1024 * 1024,	/* preferred */
	512 * 1024	/* minimum */
#endif
};
