#ifndef LOKA_TOOLBOX_TEXT_CONTEXT_HPP
#define LOKA_TOOLBOX_TEXT_CONTEXT_HPP

#include "context/ToolboxProjectedNodeContext.hpp"
#include "app/nodes/Text.hpp"
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

class ToolboxTextContext : public ToolboxProjectedNodeContext
{
public:
  explicit ToolboxTextContext(loka::app::TextNode *node);
  virtual ~ToolboxTextContext();

  void updateData(loka::core::State<loka::core::String> *text);
  void updateRect(const Rect &rect, short textX, short textY);
  short visibleWidth() const;
  void draw(ToolboxScenePlatformController *controller);
  virtual loka::core::State<loka::core::String> *projectedTextState()
  {
    return text_;
  }
  virtual void render(loka::app::scene::IPlatformController *controller);
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);

private:
  loka::app::TextNode *node_;
  Rect rect_;
  short textX_;
  short textY_;
  short maxWidth_;
  loka::app::TextWrap wrapMode_;
  loka::app::TextTruncation truncationMode_;
  loka::core::State<loka::core::String> *text_;
};

void RegisterToolboxTextNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_TOOLBOX_TEXT_CONTEXT_HPP
