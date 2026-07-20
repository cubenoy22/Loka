// SIZE partition override for the Retro68/Classic build (#135 follow-up).
// SimpleViewer decodes whole PICTs into the heap, so it keeps a larger
// partition than the other examples: 512K min / 1024K preferred (the
// toolchain's default preferred size, kept as headroom for PICT decode
// buffers). Rez resource ordering: Retro68APPL.r's SIZE (-1) is Rezzed
// first, this one is appended after and overrides it.
#include "Processes.r"

resource 'SIZE' (-1) {
	reserved,
	ignoreSuspendResumeEvents,
	reserved,
	cannotBackground,
	needsActivateOnFGSwitch,
	backgroundAndForeground,
	dontGetFrontClicks,
	ignoreChildDiedEvents,
	is32BitCompatible,
	notHighLevelEventAware,
	onlyLocalHLEvents,
	notStationeryAware,
	dontUseTextEditServices,
	reserved,
	reserved,
	reserved,
	1024 * 1024,	/* preferred */
	512 * 1024	/* minimum */
};
