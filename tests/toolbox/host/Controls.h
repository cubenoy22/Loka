#ifndef LOKA_HOST_CONTROLS_H
#define LOKA_HOST_CONTROLS_H
#include "TextEdit.h"
struct HostControl { short value; };
typedef HostControl *ControlRef;
typedef short ControlPartCode;
typedef void (*ControlActionUPP)(ControlRef, ControlPartCode);
enum { kControlIndicatorPart = 129 };
namespace toolbox_host
{
  extern ControlRef hitControl;
  extern short trackedValue;
  extern unsigned tracks;
}
inline short GetControlValue(ControlRef c) { return c->value; }
inline void SetControlValue(ControlRef c, short value) { c->value = value; }
inline void HiliteControl(ControlRef, short) {}
inline void ShowControl(ControlRef) {}
inline void HideControl(ControlRef) {}
inline short FindControl(Point, GrafPtr, ControlRef *out)
{ *out = toolbox_host::hitControl; return *out ? kControlIndicatorPart : 0; }
inline short TrackControl(ControlRef c, Point, ControlActionUPP)
{ ++toolbox_host::tracks; c->value = toolbox_host::trackedValue; return kControlIndicatorPart; }
#endif
