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
  Rect currentClip();
  struct ControlCalls
  {
    unsigned values, hilites, shows, draws;
    Rect valueClip, hiliteClip, showClip, drawClip;
    ControlCalls() : values(0), hilites(0), shows(0), draws(0),
        valueClip(), hiliteClip(), showClip(), drawClip() {}
  };
  extern ControlCalls controlCalls;
}
inline short GetControlValue(ControlRef c) { return c->value; }
inline void SetControlValue(ControlRef c, short value)
{
  ++toolbox_host::controlCalls.values;
  toolbox_host::controlCalls.valueClip = toolbox_host::currentClip();
  c->value = value;
}
inline void HiliteControl(ControlRef, short)
{
  ++toolbox_host::controlCalls.hilites;
  toolbox_host::controlCalls.hiliteClip = toolbox_host::currentClip();
}
inline void ShowControl(ControlRef)
{
  ++toolbox_host::controlCalls.shows;
  toolbox_host::controlCalls.showClip = toolbox_host::currentClip();
}
inline void Draw1Control(ControlRef)
{
  ++toolbox_host::controlCalls.draws;
  toolbox_host::controlCalls.drawClip = toolbox_host::currentClip();
}
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
