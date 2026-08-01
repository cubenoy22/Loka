#ifndef LOKA_TOOLBOX_EDIT_TEXT_CONTEXT_HPP
#define LOKA_TOOLBOX_EDIT_TEXT_CONTEXT_HPP

#include "context/ToolboxProjectedNodeContext.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "core/String.hpp"
#include <Quickdraw.h>

class ToolboxScenePlatformController;
namespace loka
{
  namespace app
  {
    namespace scene
    {
      class PlatformNodeHandlerRegistry;
    }
  } // namespace app
} // namespace loka
namespace loka
{
  namespace core
  {
    namespace scene
    {
      class BoundaryNode;
    }
  } // namespace core
} // namespace loka
namespace loka
{
  namespace core
  {
    namespace scene
    {
      class IPlatformController;
    }
  } // namespace core
} // namespace loka

class ToolboxEditTextContext : public ToolboxProjectedNodeContext
{
public:
  explicit ToolboxEditTextContext(loka::app::EditTextNode *node);
  virtual ~ToolboxEditTextContext();

  void updateData(loka::core::State<loka::core::String> *text);
  void updateRect(const Rect &outerRect, const Rect &textRect, short textX, short textY);
  void draw(ToolboxScenePlatformController *controller);
  virtual loka::core::State<loka::core::String> *projectedTextState()
  {
    return text_;
  }
  virtual void render(loka::app::scene::IPlatformController *controller);
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);

private:
  loka::app::EditTextNode *node_;
  Rect rect_;
  Rect textRect_;
  short textX_;
  short textY_;
  loka::core::State<loka::core::String> *text_;
};

bool RegisterToolboxEditTextNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_TOOLBOX_EDIT_TEXT_CONTEXT_HPP
