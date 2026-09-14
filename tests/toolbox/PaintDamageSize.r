// SIZE partition for the paint-damage standalone pin bundle (#766).
//
// LokaPaintDamage68K mounts one hidden window per pinned arm (twenty as of
// #766), each with its own scene, so it stopped fitting in the HelloWorld
// shipping partition it used to borrow (memFullErr, "type 25", before the
// first arm ran). This is a pin vehicle, not an example: it gets its own
// budget so adding an arm never measures the ceiling instead of the arm.
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
	768 * 1024,	/* preferred */
	640 * 1024	/* minimum */
#endif
};
