// SIZE partition override for SimpleText (#1131 PR 6).
// Measured need: PENDING (owner session measures with tests/toolbox/measure-example-heaps.sh before the PR)
// 68K values are provisional; PPC values follow the unmeasured template.
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
	1024 * 1024,
	1024 * 1024
#elif LOKA_CLASSIC_68K
	576 * 1024,	/* preferred */
	384 * 1024	/* minimum */
#else
	/* PPC partitions are unmeasured. */
	512 * 1024,	/* preferred */
	384 * 1024	/* minimum */
#endif
};
