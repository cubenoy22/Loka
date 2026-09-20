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
