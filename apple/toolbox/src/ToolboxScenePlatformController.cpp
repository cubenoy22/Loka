#include "ToolboxScenePlatformController.hpp"
#include "ToolboxWindow.hpp"
#include <Quickdraw.h>
#include <cstring>
#include <string>
#include "loka/platform/StringUTF8.hpp"
#include "app/Text.hpp"
#include "app/Button.hpp"
#include "app/EditText.hpp"
#include "app/RowColumn.hpp"
#include "core2/scene/Node.hpp"

namespace
{
  struct RenderState
  {
    short x;
    short y;
    short lineHeight;
    short spacing;
  };

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

  short MeasureTextWidth(const loka::core::String &value)
  {
    std::string utf8;
    if (!loka::platform::CollectUtf8(value, utf8))
    {
      return 40;
    }
    return static_cast<short>(utf8.size() * 7 + 16);
  }

  short DrawNode(declara::core::scene::Node *node, RenderState &state);

  short DrawChildren(declara::core::scene::Node *node, RenderState &state)
  {
    declara::core::scene::INestable *nestable = dynamic_cast<declara::core::scene::INestable *>(node);
    if (!nestable)
    {
      return 0;
    }
    short maxWidth = 0;
    loka::dsl::CompositionCursor<declara::core::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (declara::core::scene::Node *child = it.next(); child; child = it.next())
    {
      short width = DrawNode(child, state);
      if (width > maxWidth)
      {
        maxWidth = width;
      }
    }
    return maxWidth;
  }

  short DrawNode(declara::core::scene::Node *node, RenderState &state)
  {
    if (!node)
    {
      return 0;
    }
    if (dynamic_cast<declara::app::ColumnNode *>(node))
    {
      return DrawChildren(node, state);
    }
    if (dynamic_cast<declara::app::RowNode *>(node))
    {
      declara::core::scene::INestable *nestable = dynamic_cast<declara::core::scene::INestable *>(node);
      if (!nestable)
      {
        return 0;
      }
      short startX = state.x;
      short maxHeight = 0;
      loka::dsl::CompositionCursor<declara::core::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
      for (declara::core::scene::Node *child = it.next(); child; child = it.next())
      {
        RenderState rowState = state;
        rowState.x = startX;
        short width = DrawNode(child, rowState);
        startX = static_cast<short>(startX + width + state.spacing);
        if (rowState.y > state.y)
        {
          maxHeight = static_cast<short>(rowState.y - state.y);
        }
      }
      state.y = static_cast<short>(state.y + maxHeight + state.spacing);
      return static_cast<short>(startX - state.x);
    }
    if (declara::app::TextNode *text = dynamic_cast<declara::app::TextNode *>(node))
    {
      if (text->props.text_)
      {
        DrawStringAt(state.x, state.y, text->props.text_->get());
        short width = MeasureTextWidth(text->props.text_->get());
        state.y = static_cast<short>(state.y + state.lineHeight + state.spacing);
        return width;
      }
      return 0;
    }
    if (declara::app::ButtonNode *button = dynamic_cast<declara::app::ButtonNode *>(node))
    {
      loka::core::String label = loka::core::String::Literal("Button");
      if (button->props.text_)
      {
        label = button->props.text_->get();
      }
      short width = MeasureTextWidth(label);
      Rect rect;
      rect.left = state.x;
      rect.top = static_cast<short>(state.y - state.lineHeight + 2);
      rect.right = static_cast<short>(state.x + width);
      rect.bottom = static_cast<short>(state.y + 6);
      FrameRect(&rect);
      DrawStringAt(static_cast<short>(state.x + 4), state.y, label);
      state.y = static_cast<short>(state.y + state.lineHeight + state.spacing);
      return width;
    }
    if (declara::app::EditTextNode *edit = dynamic_cast<declara::app::EditTextNode *>(node))
    {
      loka::core::String value;
      if (edit->props.text_)
      {
        value = edit->props.text_->get();
      }
      short width = MeasureTextWidth(value);
      Rect rect;
      rect.left = state.x;
      rect.top = static_cast<short>(state.y - state.lineHeight + 2);
      rect.right = static_cast<short>(state.x + width);
      rect.bottom = static_cast<short>(state.y + 6);
      FrameRect(&rect);
      DrawStringAt(static_cast<short>(state.x + 4), state.y, value);
      state.y = static_cast<short>(state.y + state.lineHeight + state.spacing);
      return width;
    }
    return DrawChildren(node, state);
  }
}

ToolboxScenePlatformController::ToolboxScenePlatformController(ToolboxWindow *window)
    : window_(window), rootNode_(0)
{
}

ToolboxScenePlatformController::~ToolboxScenePlatformController()
{
}

void ToolboxScenePlatformController::onChange(declara::core::scene::Node *rootNode, declara::core::scene::NodeDirtyFlags)
{
  rootNode_ = rootNode;
  if (!window_ || !window_->window())
  {
    return;
  }
  window_->requestInvalidate();
}

void ToolboxScenePlatformController::synchronize()
{
  // Toolbox doesn't have a retained scene graph; rely on Update events.
}

void ToolboxScenePlatformController::destroy()
{
  rootNode_ = 0;
}

void ToolboxScenePlatformController::render()
{
  if (!window_ || !window_->window() || !rootNode_)
  {
    return;
  }
  RenderState state;
  state.x = 12;
  state.y = 24;
  state.lineHeight = 14;
  state.spacing = 6;
  DrawNode(rootNode_, state);
}
