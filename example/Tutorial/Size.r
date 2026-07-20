// SIZE partition override for the Retro68/Classic build (#135 follow-up).
// Tutorial is a small sample app; 384K min / 512K preferred replaces the
// toolchain's default 1024K/1024K template partition (Rez resource
// ordering: Retro68APPL.r's SIZE (-1) is Rezzed first, this one is
// appended after and overrides it).
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
