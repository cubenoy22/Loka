#ifndef LOKA_HOST_TEXT_EDIT_H
#define LOKA_HOST_TEXT_EDIT_H
#include "Quickdraw.h"
#include <string>
struct Point
{
  short v, h;
};
typedef char **Handle;
struct TERec
{
  Rect destRect, viewRect;
  short teLength, selStart, selEnd;
  short txFont, txSize, lineHeight, fontAscent;
  std::string text;
  char *data;
  bool autoView;
  bool active;
  int idleCalls;
};
typedef TERec **TEHandle;
TEHandle TENew(const Rect *, const Rect *);
void TEDispose(TEHandle);
void TESetText(const void *, long, TEHandle);
Handle TEGetText(TEHandle);
void TESetSelect(short, short, TEHandle);
void TEKey(char, TEHandle);
void TEClick(Point, bool, TEHandle);
void TEActivate(TEHandle);
void TEDeactivate(TEHandle);
void TEIdle(TEHandle);
bool PtInRect(Point, const Rect *);
void TEAutoView(bool, TEHandle);
void TECalText(TEHandle);
void TEUpdate(const Rect *, TEHandle);
void HLock(Handle);
void HUnlock(Handle);
void BlockMoveData(const void *, void *, long);
bool EqualRect(const Rect *, const Rect *);
void OffsetRect(Rect *, short, short);
void FrameRect(const Rect *);
namespace toolbox_host
{
  extern int copied, sets, disposals, failSets, failNew, updates;
}
#endif
