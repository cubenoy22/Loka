#include "ToolboxOutOfMemory.hpp"
#include <Dialogs.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <string>
static std::string events;
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
Ptr NewPtr(Size n) { assert(n == 8192); events += 'A'; return static_cast<Ptr>(std::malloc(n)); }
void DisposePtr(Ptr p) { assert(p); events += 'R'; std::free(p); }
Handle NewHandle(Size n) { Handle h = new Ptr; *h = static_cast<Ptr>(std::malloc(n)); return h; }
void DisposeHandle(Handle h) { std::free(*h); delete h; }
unsigned char *LMGetCurApName() { return appName; }
DialogPtr NewDialog(void *, const Rect *bounds, const unsigned char *, Boolean, short, WindowPtr, Boolean, long, Handle h)
{
  assert(events == "AR");
  dialogBounds = *bounds;
  events += 'D'; items = h;
  const unsigned char *p = reinterpret_cast<unsigned char *>(*h);
  assert(word(p) == 2); p += 2;
  assert(p[12] == ctrlItem + btnCtrl && p[13] == 4);
  assert(std::memcmp(p + 14, "Quit", 4) == 0); p += 18;
  assert(p[12] == statText + itemDisable);
  const std::string text(reinterpret_cast<const char *>(p + 14), p[13]);
  assert(text == "Not enough memory to keep Mine running. In the Finder, use Get Info to raise its memory size.");
  p += 14 + ((p[13] + 1) & ~1);
  assert(p[12] == iconItem + itemDisable && p[13] == 2 && word(p + 14) == 0);
  return &port;
}
void DisposeDialog(DialogPtr p) { assert(p == &port); events += 'X'; DisposeHandle(items); }
void SysBeep(short n) { assert(n == 1); events += 'B'; }
void ExitToShell() { events += 'Q'; }
void GetPort(GrafPtr *p) { *p = &port; }
void SetPort(GrafPtr) {}
void GetPenState(PenState *) {}
void SetPenState(const PenState *) {}
void PenNormal() {}
void PenSize(short x, short y) { assert(x == 3 && y == 3); }
void InsetRect(Rect *r, short x, short y) { r->left += x; r->right -= x; r->top += y; r->bottom -= y; }
void FrameRoundRect(const Rect *r, short x, short y)
{
  assert(x == 16 && y == 16);
  assert(r->top == 96 && r->left == 260 && r->bottom == 124 && r->right == 336);
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
  assert(!updated);
  event.what = keyDown; event.message = 13;
  const Boolean returned = filter(&port, &event, hit);
  assert(returned && *hit == 1);
  event.message = 3; *hit = 0;
  const Boolean entered = filter(&port, &event, hit);
  assert(entered && *hit == 1);
  if (++modalCalls == 1) *hit = 2;
}
int main()
{
  loka::toolbox::QuitForOutOfMemory();
  assert(events == "Q"); events.clear();
  loka::toolbox::ArmOutOfMemoryReserve();
  loka::toolbox::QuitForOutOfMemory();
  assert(events == "ARDBMMXQ");
  assert(outlines == 3);
  // 640x480: centred, a third of the way down below the 20-pixel menu bar.
  assert(dialogBounds.left == 144 && dialogBounds.right == 496);
  assert(dialogBounds.top == 126 && dialogBounds.bottom == 266);
  events.clear();
  loka::toolbox::QuitForOutOfMemory();
  assert(events == "Q");
  // A 512x342 screen (Mac Plus) keeps the whole alert on screen.
  qd.screenBits.bounds.bottom = 342; qd.screenBits.bounds.right = 512;
  events.clear(); modalCalls = 0;
  loka::toolbox::ArmOutOfMemoryReserve();
  loka::toolbox::QuitForOutOfMemory();
  assert(events == "ARDBMMXQ");
  assert(dialogBounds.left == 80 && dialogBounds.right == 432);
  assert(dialogBounds.top == 80 && dialogBounds.bottom == 220);
}
