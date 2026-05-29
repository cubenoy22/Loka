#ifndef LOKA_TOOLBOX_EDIT_TEXT_CONTEXT_HPP
#define LOKA_TOOLBOX_EDIT_TEXT_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "core/String.hpp"
#include <Quickdraw.h>

class ToolboxScenePlatformController;
namespace loka
{
  namespace core
  {
    namespace scene
    {
      class BoundaryNode;
    }
  }
}
namespace loka
{
  namespace core
  {
    namespace scene
    {
      class IPlatformController;
    }
  }
}

class ToolboxEditTextContext : public loka::app::scene::NativeNodeContext
{
public:
  explicit ToolboxEditTextContext(loka::app::EditTextNode *node);
  virtual ~ToolboxEditTextContext();

  void setBoundary(loka::app::scene::BoundaryNode *boundary) { boundary_ = boundary; }
  void updateData(loka::core::State<loka::core::String> *text);
  void updateRect(const Rect &outerRect, const Rect &textRect, short textX, short textY);
  void draw(ToolboxScenePlatformController *controller);
  virtual void render(loka::app::scene::IPlatformController *controller);
  virtual short layout(loka::app::scene::IPlatformController *controller,
                       loka::app::scene::LayoutState &state);

private:
  loka::app::EditTextNode *node_;
  loka::app::scene::BoundaryNode *boundary_;
  Rect rect_;
  Rect textRect_;
  short textX_;
  short textY_;
  loka::core::State<loka::core::String> *text_;
};

#endif // LOKA_TOOLBOX_EDIT_TEXT_CONTEXT_HPP
