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
#ifdef LOKA_HOST_CONTROL_WIDTH
namespace toolbox_host { extern Rect controlRect; }
struct PenState {};
struct HostQD { int gray; };
extern HostQD qd;
inline void GetPenState(PenState *) {}
inline void SetPenState(const PenState *) {}
inline void PenPat(const int *) {}
inline void SetControlTitle(ControlRef, const unsigned char *) {}
inline void HiliteControl(ControlRef, short) {}
inline void Draw1Control(ControlRef) {}
inline void LineTo(short, short) {}
typedef int *MenuHandle;
inline MenuHandle NewMenu(short, const unsigned char *) { return new int(0); }
inline void AppendMenu(MenuHandle, const unsigned char *) {}
inline void InsertMenu(MenuHandle, short) {}
inline void LocalToGlobal(Point *) {}
inline long PopUpMenuSelect(MenuHandle, short, short, short) { return 2; }
inline void DeleteMenu(short) {}
inline void DisposeMenu(MenuHandle menu) { delete menu; }
#endif
#endif
