#include "ToolboxOutOfMemory.hpp"
#include <Dialogs.h>
#include <Events.h>
#include <LowMem.h>
#include <Memory.h>
#include <Processes.h>
#include <Quickdraw.h>
#include <Sound.h>
#include <cstring>

namespace {
const long kOutOfMemoryReserveBytes = 8 * 1024;
// Process-owned BSS: absence also guards reporting before initialization/reentry.
Ptr gOutOfMemoryReserve;
const Rect kQuitBounds = {100, 264, 120, 332};

void PutWord(unsigned char *&out, unsigned short value)
{
  *out++ = static_cast<unsigned char>(value >> 8);
  *out++ = static_cast<unsigned char>(value);
}

// DITL records have a four-byte placeholder, Rect, type, byte length, and
// even-padded data. Encode explicitly: host pointer size/alignment is irrelevant.
void PutItem(unsigned char *&out, const Rect &bounds, unsigned char type,
             const unsigned char *data, unsigned char length)
{
  PutWord(out, 0); PutWord(out, 0);
  PutWord(out, bounds.top); PutWord(out, bounds.left);
  PutWord(out, bounds.bottom); PutWord(out, bounds.right);
  *out++ = type; *out++ = length;
  std::memcpy(out, data, length); out += length;
  if (length & 1) *out++ = 0;
}

void DrawQuitOutline(DialogPtr dialog)
{
  GrafPtr savedPort;
  GetPort(&savedPort);
  SetPort(reinterpret_cast<GrafPtr>(dialog));
  PenState savedPen;
  GetPenState(&savedPen);
  PenNormal();
  PenSize(3, 3);
  Rect bounds = kQuitBounds;
  InsetRect(&bounds, -4, -4);
  FrameRoundRect(&bounds, 16, 16);
  SetPenState(&savedPen);
  SetPort(savedPort);
}

pascal Boolean QuitFilter(DialogPtr dialog, EventRecord *event, short *item)
{
  if (event->what == updateEvt &&
      static_cast<unsigned long>(event->message) == reinterpret_cast<unsigned long>(dialog))
  {
    // Draw the standard default outline inside the update clip, after items.
    BeginUpdate(reinterpret_cast<WindowPtr>(dialog));
    DrawDialog(dialog);
    DrawQuitOutline(dialog);
    EndUpdate(reinterpret_cast<WindowPtr>(dialog));
    return false;
  }
  if (event->what == keyDown || event->what == autoKey)
  {
    const unsigned char key = event->message & charCodeMask;
    if (key == 13 || key == 3)
    {
      *item = 1;
      return true;
    }
  }
  return false;
}

void ShowOutOfMemoryDialog()
{
  const char prefix[] = "Not enough memory to keep ";
  const char suffix[] = " running. In the Finder, use Get Info to raise its memory size.";
  unsigned char text[255];
  unsigned char name[32];
  BlockMoveData(LMGetCurApName(), name, sizeof(name));
  const unsigned int fixedLength = sizeof(prefix) + sizeof(suffix) - 2;
  unsigned int nameLength = name[0];
  if (nameLength > sizeof(name) - 1) nameLength = sizeof(name) - 1;
  if (nameLength > sizeof(text) - fixedLength)
    nameLength = sizeof(text) - fixedLength;
  std::memcpy(text, prefix, sizeof(prefix) - 1);
  std::memcpy(text + sizeof(prefix) - 1, name + 1, nameLength);
  std::memcpy(text + sizeof(prefix) - 1 + nameLength, suffix, sizeof(suffix) - 1);
  const unsigned char textLength = static_cast<unsigned char>(fixedLength + nameLength);

  unsigned char bytes[2 + 18 + 14 + 256 + 16];
  unsigned char *out = bytes;
  PutWord(out, 2); // count minus one
  PutItem(out, kQuitBounds, ctrlItem + btnCtrl,
          reinterpret_cast<const unsigned char *>("Quit"), 4);
  const Rect textBounds = {20, 64, 88, 332};
  PutItem(out, textBounds, statText + itemDisable, text, textLength);
  const Rect iconBounds = {20, 20, 52, 52};
  const unsigned char stopIcon[] = {0, 0}; // System ICON resource 0
  PutItem(out, iconBounds, iconItem + itemDisable, stopIcon, 2);
  Handle items = NewHandle(out - bytes);
  if (!items) return;
  std::memcpy(*items, bytes, out - bytes);
  // Centre horizontally and place a third of the way down the work area, as
  // the Dialog Manager places alerts; items are in dialog-local coordinates.
  const short width = 352, height = 140;
  const Rect &screen = qd.screenBits.bounds;
  const short workTop = static_cast<short>(screen.top + GetMBarHeight());
  const short left = static_cast<short>((screen.left + screen.right - width) / 2);
  const short top = static_cast<short>(workTop + (screen.bottom - workTop - height) / 3);
  const Rect bounds = {top, left, static_cast<short>(top + height), static_cast<short>(left + width)};
  const unsigned char title[] = {0};
  DialogPtr dialog = NewDialog(0, &bounds, title, true, dBoxProc,
                              reinterpret_cast<WindowPtr>(-1), false, 0, items);
  if (!dialog)
  {
    DisposeHandle(items);
    return;
  }
  // NewDialog transfers the item list to the dialog; DisposeDialog owns it.
  ModalFilterUPP filter = NewModalFilterUPP(QuitFilter);
  DrawQuitOutline(dialog);
  SysBeep(1);
  short item = 0;
  do { ModalDialog(filter, &item); } while (item != 1);
  DisposeModalFilterUPP(filter);
  DisposeDialog(dialog);
}
}

namespace loka { namespace toolbox {
void ArmOutOfMemoryReserve()
{
  gOutOfMemoryReserve = NewPtr(kOutOfMemoryReserveBytes);
}

void QuitForOutOfMemory()
{
  if (!gOutOfMemoryReserve)
  {
    ExitToShell();
#if defined(LOKA_RETRO68)
    __builtin_unreachable();
#else
    return; // Host ExitToShell recorder returns.
#endif
  }
  DisposePtr(gOutOfMemoryReserve);
  gOutOfMemoryReserve = 0;
  ShowOutOfMemoryDialog();
  ExitToShell();
#if defined(LOKA_RETRO68)
  __builtin_unreachable();
#endif
}
} }
