#include "context/ToolboxPopupMenuContext.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "loka/platform/StringUTF8.hpp"
#include <cstring>
#include <string>

namespace
{
  void DrawStringAt(short x, short y, const loka::core::String &value)
  {
    std::string utf8;
    if (!loka::platform::CollectUtf8(value, utf8))
    {
      return;
    }
    std::size_t length = utf8.size();
    if (length > 255)
    {
      length = 255;
    }
    Str255 text;
    text[0] = static_cast<unsigned char>(length);
    if (length > 0)
    {
      std::memcpy(text + 1, utf8.data(), length);
    }
    MoveTo(x, y);
    DrawString(text);
  }
}

ToolboxPopupMenuContext::ToolboxPopupMenuContext(declara::app::PopupMenuNode *node)
    : node_(node),
      rect_(),
      items_(0),
      selectedIndex_(0),
      onChange_(0),
      enabled_(0)
{
}

ToolboxPopupMenuContext::~ToolboxPopupMenuContext() {}

void ToolboxPopupMenuContext::updateData(const loka::Vector<loka::core::String> *items,
                                         declara::core::State<int> *selectedIndex,
                                         declara::core::EmitterState *onChange,
                                         declara::core::State<bool> *enabled)
{
  items_ = items;
  selectedIndex_ = selectedIndex;
  onChange_ = onChange;
  enabled_ = enabled;
}

void ToolboxPopupMenuContext::updateRect(const Rect &rect)
{
  rect_ = rect;
}

short ToolboxPopupMenuContext::clampIndex(int index) const
{
  if (!items_ || items_->size() == 0)
  {
    return 0;
  }
  if (index < 0)
  {
    return 0;
  }
  if (static_cast<std::size_t>(index) >= items_->size())
  {
    return static_cast<short>(items_->size() - 1);
  }
  return static_cast<short>(index);
}

void ToolboxPopupMenuContext::copyToPascalString(const loka::core::String &value, Str255 out) const
{
  std::string utf8;
  if (!loka::platform::CollectUtf8(value, utf8))
  {
    out[0] = 0;
    return;
  }
  std::size_t length = utf8.size();
  if (length > 255)
  {
    length = 255;
  }
  out[0] = static_cast<unsigned char>(length);
  if (length > 0)
  {
    std::memcpy(out + 1, utf8.data(), length);
  }
}

void ToolboxPopupMenuContext::draw(short lineHeight)
{
  if (!node_)
  {
    return;
  }
  loka::core::String label = loka::core::String::Literal("Select");
  int selectedIndex = 0;
  if (selectedIndex_)
  {
    selectedIndex = selectedIndex_->get();
  }
  if (items_ && items_->size() > 0)
  {
    short clamped = clampIndex(selectedIndex);
    label = (*items_)[clamped];
  }
  PenState penState;
  GetPenState(&penState);
  FrameRect(&rect_);
  PenPat(&qd.gray);
  MoveTo(rect_.left + 2, rect_.bottom);
  LineTo(rect_.right, rect_.bottom);
  LineTo(rect_.right, rect_.top + 2);
  SetPenState(&penState);
  short textY = static_cast<short>(rect_.top + lineHeight - 2);
  DrawStringAt(static_cast<short>(rect_.left + 4), textY, label);
  short arrowRight = static_cast<short>(rect_.right - 4);
  short arrowTop = static_cast<short>(rect_.top + 4);
  short arrowBottom = static_cast<short>(rect_.bottom - 4);
  short arrowMidY = static_cast<short>((arrowTop + arrowBottom) / 2);
  PolyHandle arrow = OpenPoly();
  MoveTo(static_cast<short>(arrowRight - 6), arrowMidY - 3);
  LineTo(arrowRight, arrowMidY - 3);
  LineTo(static_cast<short>(arrowRight - 3), arrowMidY + 3);
  LineTo(static_cast<short>(arrowRight - 6), arrowMidY - 3);
  ClosePoly();
  PaintPoly(arrow);
  KillPoly(arrow);
}

bool ToolboxPopupMenuContext::handleMouseDown(const Point &point, ToolboxScenePlatformController *controller)
{
  if (enabled_ && !enabled_->get())
  {
    return false;
  }
  if (!items_ || items_->size() == 0 || !selectedIndex_)
  {
    return false;
  }
  if (!PtInRect(point, &rect_))
  {
    return false;
  }
  MenuHandle menu = NewMenu(2000, "\p");
  if (!menu)
  {
    return false;
  }
  for (std::size_t j = 0; j < items_->size(); ++j)
  {
    Str255 text;
    copyToPascalString((*items_)[j], text);
    AppendMenu(menu, text);
  }
  InsertMenu(menu, -1);
  short currentIndex = clampIndex(selectedIndex_->get());
  Point globalPoint = point;
  LocalToGlobal(&globalPoint);
  long choice = PopUpMenuSelect(menu, globalPoint.v, globalPoint.h, static_cast<short>(currentIndex + 1));
  short item = static_cast<short>(choice & 0xFFFF);
  if (item > 0 && controller)
  {
    controller->applyPopupSelectionChange(rect_, selectedIndex_, onChange_, static_cast<int>(item - 1));
  }
  DeleteMenu(2000);
  DisposeMenu(menu);
  return true;
}
