#include "ToolboxOutOfMemory.hpp"
#include <Dialogs.h>
#include "support/TestVerify.hpp"
#include <cstdlib>
#include <cstring>
#include <string>
static std::string events;
static Zone zone = {0};
static int installs, uppCreations;
static bool refuseReserve, refuseItems, reenterReporting;
Zone *GetZone() { return &zone; }
void SetGrowZone(GrowZoneUPP procedure)
{
  LOKA_VERIFY(events == "A" && procedure);
  zone.gzProc = procedure;
  ++installs;
}
GrowZoneUPP NewGrowZoneUPP(GrowZoneUPP procedure)
{
  LOKA_VERIFY(events == "A");
  ++uppCreations;
  return procedure;
}
static Handle items;
static Port port;
static int modalCalls;
static int outlines;
static unsigned char appName[32] = {4, 'M', 'i', 'n', 'e'};
QDGlobals qd = {{{0, 0, 480, 640}}};
short GetMBarHeight() { return 20; }
static Rect dialogBounds;
static unsigned short word(const unsigned char *p) { return (p[0] << 8) | p[1]; }
void BlockMoveData(const void *src, void *dst, Size n) { std::memcpy(dst, src, n); }
Ptr NewPtr(Size n) { LOKA_VERIFY(n == 8192); events += 'A'; return refuseReserve ? 0 : static_cast<Ptr>(std::malloc(n)); }
void DisposePtr(Ptr p) { LOKA_VERIFY(p); events += 'R'; std::free(p); }
Handle NewHandle(Size n)
{
  if (reenterReporting)
  {
    const std::string before = events;
    loka::toolbox::QuitForOutOfMemory();
    LOKA_VERIFY(events == before + "Q");
    return 0; // The real nested ExitToShell never returns.
  }
  if (refuseItems) return 0;
  Handle h = new Ptr;
  *h = static_cast<Ptr>(std::malloc(n));
  return h;
}
void DisposeHandle(Handle h) { std::free(*h); delete h; }
unsigned char *LMGetCurApName() { return appName; }
DialogPtr NewDialog(void *, const Rect *bounds, const unsigned char *, Boolean, short, WindowPtr, Boolean, long, Handle h)
{
  LOKA_VERIFY(events == "AR");
  dialogBounds = *bounds;
  events += 'D'; items = h;
  const unsigned char *p = reinterpret_cast<unsigned char *>(*h);
  LOKA_VERIFY(word(p) == 2); p += 2;
  LOKA_VERIFY(p[12] == ctrlItem + btnCtrl && p[13] == 4);
  LOKA_VERIFY(std::memcmp(p + 14, "Quit", 4) == 0); p += 18;
  LOKA_VERIFY(p[12] == statText + itemDisable);
  const std::string text(reinterpret_cast<const char *>(p + 14), p[13]);
  LOKA_VERIFY(text == "Not enough memory to keep Mine running. In the Finder, use Get Info to raise its memory size.");
  p += 14 + ((p[13] + 1) & ~1);
  LOKA_VERIFY(p[12] == iconItem + itemDisable && p[13] == 2 && word(p + 14) == 0);
  return &port;
}
void DisposeDialog(DialogPtr p) { LOKA_VERIFY(p == &port); events += 'X'; DisposeHandle(items); }
void SysBeep(short n) { LOKA_VERIFY(n == 1); events += 'B'; }
void ExitToShell() { events += 'Q'; }
void GetPort(GrafPtr *p) { *p = &port; }
void SetPort(GrafPtr) {}
void GetPenState(PenState *) {}
void SetPenState(const PenState *) {}
void PenNormal() {}
void PenSize(short x, short y) { LOKA_VERIFY(x == 3 && y == 3); }
void InsetRect(Rect *r, short x, short y) { r->left += x; r->right -= x; r->top += y; r->bottom -= y; }
void FrameRoundRect(const Rect *r, short x, short y)
{
  LOKA_VERIFY(x == 16 && y == 16);
  LOKA_VERIFY(r->top == 96 && r->left == 260 && r->bottom == 124 && r->right == 336);
  ++outlines;
}
void DrawDialog(DialogPtr) {}
void BeginUpdate(WindowPtr) {}
void EndUpdate(WindowPtr) {}
void ModalDialog(ModalFilterUPP filter, short *hit)
{
  events += 'M';
  EventRecord event = {updateEvt, reinterpret_cast<unsigned long>(&port)};
  const Boolean updated = filter(&port, &event, hit);
  LOKA_VERIFY(!updated);
  event.what = keyDown; event.message = 13;
  const Boolean returned = filter(&port, &event, hit);
  LOKA_VERIFY(returned && *hit == 1);
  event.message = 3; *hit = 0;
  const Boolean entered = filter(&port, &event, hit);
  LOKA_VERIFY(entered && *hit == 1);
  if (++modalCalls == 1) *hit = 2;
}
int main()
{
  loka::toolbox::QuitIfOutOfMemoryReserveSpent();
  LOKA_VERIFY(events.empty());
  loka::toolbox::QuitForOutOfMemory();
  LOKA_VERIFY(events == "Q"); events.clear();
  refuseReserve = true;
  loka::toolbox::ArmOutOfMemoryReserve();
  LOKA_VERIFY(events == "A" && installs == 0 && uppCreations == 0 && !zone.gzProc);
  loka::toolbox::QuitIfOutOfMemoryReserveSpent();
  LOKA_VERIFY(events == "A");
  loka::toolbox::QuitForOutOfMemory();
  LOKA_VERIFY(events == "AQ");
  events.clear(); refuseReserve = false;
  loka::toolbox::ArmOutOfMemoryReserve();
  LOKA_VERIFY(installs == 1 && uppCreations == 1 && zone.gzProc);
  loka::toolbox::QuitIfOutOfMemoryReserveSpent();
  LOKA_VERIFY(events == "A");
  LOKA_VERIFY(zone.gzProc(8193) == 0 && events == "A");
  loka::toolbox::QuitForOutOfMemory();
  LOKA_VERIFY(events == "ARDBMMXQ");
  LOKA_VERIFY(outlines == 3);
  // 640x480: centred, a third of the way down below the 20-pixel menu bar.
  LOKA_VERIFY(dialogBounds.left == 144 && dialogBounds.right == 496);
  LOKA_VERIFY(dialogBounds.top == 126 && dialogBounds.bottom == 266);
  events.clear();
  loka::toolbox::QuitForOutOfMemory();
  LOKA_VERIFY(events == "Q");
  // A 512x342 screen (Mac Plus) keeps the whole alert on screen.
  qd.screenBits.bounds.bottom = 342; qd.screenBits.bounds.right = 512;
  events.clear(); modalCalls = 0;
  loka::toolbox::ArmOutOfMemoryReserve();
  loka::toolbox::QuitForOutOfMemory();
  LOKA_VERIFY(events == "ARDBMMXQ");
  LOKA_VERIFY(dialogBounds.left == 80 && dialogBounds.right == 432);
  LOKA_VERIFY(dialogBounds.top == 80 && dialogBounds.bottom == 220);
  // Each rearm below simulates another process after the host exit recorder.
  for (int mode = 0; mode < 2; ++mode)
  {
    events.clear(); modalCalls = 0;
    const int previousInstalls = installs;
    loka::toolbox::ArmOutOfMemoryReserve();
    LOKA_VERIFY(installs == previousInstalls + 1 && uppCreations == 1);
    LOKA_VERIFY(zone.gzProc(mode == 0 ? 10 : 8192) == 8192);
    LOKA_VERIFY(events == "AR");
    LOKA_VERIFY(zone.gzProc(10) == 0 && events == "AR");
    if (mode == 0) loka::toolbox::QuitIfOutOfMemoryReserveSpent();
    else loka::toolbox::QuitForOutOfMemory();
    LOKA_VERIFY(events == "ARDBMMXQ");
    events.clear();
    loka::toolbox::QuitIfOutOfMemoryReserveSpent();
    LOKA_VERIFY(events.empty());
  }
  events.clear();
  loka::toolbox::ArmOutOfMemoryReserve();
  reenterReporting = true;
  loka::toolbox::QuitForOutOfMemory();
  LOKA_VERIFY(events == "ARQQ");
  reenterReporting = false;
  events.clear();
  loka::toolbox::ArmOutOfMemoryReserve();
  refuseItems = true;
  loka::toolbox::QuitForOutOfMemory();
  LOKA_VERIFY(events == "ARQ");
}
