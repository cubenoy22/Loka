// SIZE partition override for the Retro68/Classic build (#135 follow-up).
// Post-diet HelloWorld runs comfortably in 384K min / 512K preferred; this
// replaces the toolchain's default 1024K/1024K template partition, which
// Rez resource ordering guarantees (Retro68APPL.r's SIZE (-1) is Rezzed
// first, this one is appended after and overrides it).
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
	512 * 1024,	/* preferred */
	384 * 1024	/* minimum */
};
