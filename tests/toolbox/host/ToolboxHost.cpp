#include "ToolboxHost.hpp"
#include <algorithm>
#include <cstring>
namespace toolbox_host
{
  std::vector<Draw> draws;
  int erases = 0, widths = 0, measures = 0, fonts = 0, metrics = 0;
  int failRegions = 0;
  void reset()
  {
    draws.clear();
    erases = widths = measures = fonts = metrics = 0;
    failRegions = 0;
  }
} // namespace toolbox_host
namespace
{
  GrafPort initialPort = {7, 12, 0};
  GrafPtr port = &initialPort;
  Rect clip = {-30000, -30000, 30000, 30000};
  short penX, penY;
  Rect Intersection(const Rect &a, const Rect &b)
  {
    Rect r = {
        std::max(a.top, b.top), std::max(a.left, b.left), std::min(a.bottom, b.bottom), std::min(a.right, b.right)};
    return r;
  }
} // namespace
bool ToolboxScenePlatformController::intersectWithProjectionClip(const Rect &rect, Rect &out) const
{
  out = Intersection(rect, projectionClip);
  return !EmptyRect(&out);
}
void GetPort(GrafPtr *out)
{
  *out = port;
}
void SetPort(GrafPtr value)
{
  port = value;
}
void TextFont(short value)
{
  port->txFont = value;
  ++toolbox_host::fonts;
}
void TextSize(short value)
{
  port->txSize = value;
}
void TextFace(Style value)
{
  port->txFace = value;
}
short GetDefFontSize()
{
  return 12;
}
short GetSysFont()
{
  return 0;
}
void GetFontInfo(FontInfo *out)
{
  ++toolbox_host::metrics;
  out->ascent = port->txSize;
  out->descent = 3;
  out->leading = 2;
  out->widMax = port->txSize / 3 + ((port->txFace & bold) ? 1 : 0);
}
void MeasureText(short count, const void *, void *charLocs)
{
  ++toolbox_host::measures;
  short *positions = static_cast<short *>(charLocs);
  for (int i = 0; i <= count; ++i)
    positions[i] = static_cast<short>(i * (port->txSize / 3 + ((port->txFace & bold) ? 1 : 0)));
}
short TextWidth(const void *, short, short length)
{
  ++toolbox_host::widths;
  return static_cast<short>(length * (port->txSize / 3 + ((port->txFace & bold) ? 1 : 0)));
}
short StringWidth(const unsigned char *text)
{
  return TextWidth(text + 1, 0, text[0]);
}
void MoveTo(short x, short y)
{
  penX = x;
  penY = y;
}
void DrawText(const void *bytes, short offset, short length)
{
  const toolbox_host::Draw draw = {
      penX, penY, port->txSize, port->txFace, length, std::string(static_cast<const char *>(bytes) + offset, length)};
  toolbox_host::draws.push_back(draw);
}
void SetRect(Rect *out, short l, short t, short r, short b)
{
  out->left = l;
  out->top = t;
  out->right = r;
  out->bottom = b;
}
bool EmptyRect(const Rect *r)
{
  return r->left >= r->right || r->top >= r->bottom;
}
RgnHandle NewRgn()
{
  if (toolbox_host::failRegions > 0)
  {
    --toolbox_host::failRegions;
    return 0;
  }
  RgnHandle r = new Region *;
  *r = new Region;
  (*r)->rgnSize = sizeof(Region);
  return r;
}
void DisposeRgn(RgnHandle r)
{
  delete *r;
  delete r;
}
void GetClip(RgnHandle r)
{
  (*r)->rgnBBox = clip;
}
void RectRgn(RgnHandle r, const Rect *rect)
{
  (*r)->rgnBBox = *rect;
}
void SectRgn(RgnHandle a, RgnHandle b, RgnHandle out)
{
  (*out)->rgnBBox = Intersection((*a)->rgnBBox, (*b)->rgnBBox);
}
void SetClip(RgnHandle r)
{
  clip = (*r)->rgnBBox;
}
bool RectInRgn(const Rect *r, RgnHandle region)
{
  const Rect both = Intersection(*r, (*region)->rgnBBox);
  return !EmptyRect(&both);
}
void EraseRect(const Rect *)
{
  ++toolbox_host::erases;
}

void ClipRect(const Rect *rect)
{
  clip = *rect;
}

#include "context/ToolboxTextEditorContext.hpp"
namespace toolbox_host
{
  int copied = 0, sets = 0, disposals = 0, failSets = 0, failNew = 0, updates = 0;
}
TEHandle TENew(const Rect *dest, const Rect *view)
{
  if (toolbox_host::failNew)
  {
    --toolbox_host::failNew;
    return 0;
  }
  TEHandle te = new TERec *;
  *te = new TERec;
  (**te).txFont = port->txFont;
  (**te).txSize = port->txSize;
  FontInfo metrics;
  GetFontInfo(&metrics);
  (**te).lineHeight = metrics.ascent + metrics.descent + metrics.leading;
  (**te).fontAscent = metrics.ascent;
  (**te).destRect = *dest;
  (**te).viewRect = *view;
  (**te).teLength = (**te).selStart = (**te).selEnd = 0;
  (**te).data = 0;
  (**te).autoView = false;
  return te;
}
void TEDispose(TEHandle te)
{
  ++toolbox_host::disposals;
  delete *te;
  delete te;
}
void TESetText(const void *bytes, long length, TEHandle te)
{
  ++toolbox_host::sets;
  if (toolbox_host::failSets)
  {
    --toolbox_host::failSets;
    length = 0;
  }
  (**te).text.assign(static_cast<const char *>(bytes), length);
  (**te).teLength = static_cast<short>(length);
}
Handle TEGetText(TEHandle te)
{
  (**te).data = const_cast<char *>((**te).text.data());
  return &(**te).data;
}
void TESetSelect(short a, short b, TEHandle te)
{
  (**te).selStart = a;
  (**te).selEnd = b;
}
void TEKey(char key, TEHandle te)
{
  TERec &t = **te;
  if (key >= 28 && key <= 31)
  {
    t.selStart = std::max(0, std::min(static_cast<int>(t.teLength), t.selStart + (key == 28 ? -1 : 1)));
    t.selEnd = t.selStart;
    return;
  }
  if (t.selStart != t.selEnd)
    t.text.erase(t.selStart, t.selEnd - t.selStart);
  else if (key == '\b' && t.selStart)
  {
    --t.selStart;
    t.text.erase(t.selStart, 1);
  }
  if (key != '\b')
    t.text.insert(t.selStart++, 1, key);
  t.selEnd = t.selStart;
  t.teLength = static_cast<short>(t.text.size());
}
void TEClick(Point p, bool, TEHandle te)
{
  short offset = 0;
  int row = std::max(0, (p.v - (**te).viewRect.top) / 16);
  for (; offset < (**te).teLength && row; ++offset)
    if ((**te).text[offset] == '\r')
      --row;
  offset = std::min(static_cast<int>((**te).teLength), offset + std::max(0, (p.h - (**te).viewRect.left) / 6));
  TESetSelect(offset, offset, te);
}
void TEAutoView(bool value, TEHandle te)
{
  (**te).autoView = value;
}
void TECalText(TEHandle) {}
void TEUpdate(const Rect *, TEHandle)
{
  ++toolbox_host::updates;
}
void HLock(Handle) {}
void HUnlock(Handle) {}
void BlockMoveData(const void *source, void *dest, long length)
{
  toolbox_host::copied += length;
  std::memmove(dest, source, length);
}
bool EqualRect(const Rect *a, const Rect *b)
{
  return std::memcmp(a, b, sizeof(Rect)) == 0;
}
void OffsetRect(Rect *r, short x, short y)
{
  r->left += x;
  r->right += x;
  r->top += y;
  r->bottom += y;
}
void FrameRect(const Rect *) {}
#include "ToolboxTextEditorBinding.cpp"
void ToolboxScenePlatformController::retireTextEditorControl(loka::app::scene::NodeContext *context,
                                                             loka::app::scene::NativeLifetimeHint)
{
  std::size_t index = 0;
  if (!this->editControls_.find(context, index))
    return;
  static_cast<ToolboxTextEditorContext *>(context)->invalidateNativePresentation();
  this->retiredTE.push_back(this->editControls_[index].te);
  this->editControls_.erase(index);
}
void ToolboxScenePlatformController::flushTE()
{
  for (std::size_t i = 0; i < this->retiredTE.size(); ++i)
    TEDispose(this->retiredTE[i]);
  this->retiredTE.clear();
}
