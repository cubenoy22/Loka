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

ToolboxPopupMenuContext::ToolboxPopupMenuContext(loka::app::PopupMenuNode *node)
    : node_(node),
      boundary_(0),
      rect_(),
      lineHeight_(0),
      items_(0),
      selectedIndex_(0),
      onChange_(0),
      enabled_(0)
{
}

ToolboxPopupMenuContext::~ToolboxPopupMenuContext() {}

void ToolboxPopupMenuContext::invalidate()
{
  node_ = 0;
  boundary_ = 0;
  items_ = 0;
  selectedIndex_ = 0;
  onChange_ = 0;
  enabled_ = 0;
  lineHeight_ = 0;
  rect_.left = rect_.top = rect_.right = rect_.bottom = 0;
}

void ToolboxPopupMenuContext::updateData(const loka::Vector<loka::core::String> *items,
                                         loka::core::State<int> *selectedIndex,
                                         loka::core::EmitterState *onChange,
                                         loka::core::State<bool> *enabled)
{
  items_ = items;
  selectedIndex_ = selectedIndex;
  onChange_ = onChange;
  enabled_ = enabled;
}

void ToolboxPopupMenuContext::updateRect(const Rect &rect, short lineHeight)
{
  rect_ = rect;
  lineHeight_ = lineHeight;
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

void ToolboxPopupMenuContext::draw()
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
  short textY = static_cast<short>(rect_.top + lineHeight_ - 2);
  DrawStringAt(static_cast<short>(rect_.left + 4), textY, label);
  short arrowRight = static_cast<short>(rect_.right - 4);
  short arrowTop = static_cast<short>(rect_.top + 4);
  short arrowBottom = static_cast<short>(rect_.bottom - 4);
  short arrowMidY = static_cast<short>((arrowTop + arrowBottom) / 2);
  MoveTo(static_cast<short>(arrowRight - 6), arrowMidY - 3);
  LineTo(arrowRight, arrowMidY - 3);
  LineTo(static_cast<short>(arrowRight - 3), arrowMidY + 3);
  LineTo(static_cast<short>(arrowRight - 6), arrowMidY - 3);
}

short ToolboxPopupMenuContext::layout(loka::app::scene::IPlatformController *controller,
                                      loka::app::scene::LayoutState &state)
{
  if (!node_)
  {
    return 0;
  }
  short width = 120;
  Rect rect;
  rect.left = state.x;
  rect.top = static_cast<short>(state.y - state.lineHeight + 2);
  rect.right = static_cast<short>(state.x + width + 8);
  rect.bottom = static_cast<short>(state.y + 6);
  updateData(node_->props.items_, node_->props.selectedIndex_, node_->props.onChange_, node_->props.enabled_);
  updateRect(rect, state.lineHeight);
  ToolboxScenePlatformController *toolbox = static_cast<ToolboxScenePlatformController *>(controller);
  if (toolbox)
  {
    toolbox->registerPopupContext(this);
  }
  state.y = static_cast<short>(state.y + state.lineHeight + state.spacing);
  return width;
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
  const loka::Vector<loka::core::String> *items = items_;
  loka::core::State<int> *selectedIndex = selectedIndex_;
  loka::core::EmitterState *onChange = onChange_;
  loka::app::scene::BoundaryNode *boundary = boundary_;
  Rect rect = rect_;
  short menuIdValue = menuId();
  MenuHandle menu = NewMenu(menuIdValue, "\p");
  if (!menu)
  {
    return false;
  }
  for (std::size_t j = 0; j < items->size(); ++j)
  {
    Str255 text;
    copyToPascalString((*items)[j], text);
    AppendMenu(menu, text);
  }
  InsertMenu(menu, -1);
  short currentIndex = clampIndex(selectedIndex->get());
  Point globalPoint = point;
  LocalToGlobal(&globalPoint);
  long choice = PopUpMenuSelect(menu, globalPoint.v, globalPoint.h, static_cast<short>(currentIndex + 1));
  short item = static_cast<short>(choice & 0xFFFF);
  if (item > 0 && controller)
  {
    controller->applyPopupSelectionChange(rect, boundary, selectedIndex, onChange, static_cast<int>(item - 1));
  }
  DeleteMenu(menuIdValue);
  DisposeMenu(menu);
  return true;
}

void ToolboxPopupMenuContext::render(loka::app::scene::IPlatformController *)
{
  draw();
}

short ToolboxPopupMenuContext::menuId() const
{
  if (node_ && node_->props.controlTag_ > 0 && node_->props.controlTag_ <= 32767)
  {
    return static_cast<short>(node_->props.controlTag_);
  }
  return 2000;
}
