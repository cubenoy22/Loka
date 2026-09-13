#ifndef LOKA_TOOLBOX_TEXT_CONTEXT_HPP
#define LOKA_TOOLBOX_TEXT_CONTEXT_HPP

#include "context/ToolboxProjectedNodeContext.hpp"
#include "app/nodes/Text.hpp"
#include "core/String.hpp"
#include "ToolboxPropsRefresh.hpp"
#include "context/ToolboxPaintSupport.hpp"
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
  ToolboxTextContext(loka::app::TextNode *node, ToolboxScenePlatformController *controller);
  virtual ~ToolboxTextContext();
  virtual void onPropsApplied();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);
  virtual loka::app::scene::PaintAnswer queryPaintDamage(const loka::app::scene::PaintQuery &query) const;
  /** Repaint the captured visible placement without registering another hit. */
  void repaint();

  void updateData(loka::core::State<loka::core::String> *text);
  void updateRect(const Rect &rect, short textX, short textY);
  short visibleWidth() const;
  loka::core::State<loka::core::String> *liveTextState() const
  {
    return this->node_ ? ToolboxLiveTextSource(this->text_, this->node_->props.ownsText) : 0;
  }
  void draw(ToolboxScenePlatformController *controller);
  virtual loka::core::State<loka::core::String> *projectedTextState()
  {
    return text_;
  }
  virtual void render(loka::app::scene::IPlatformController *controller);
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);

private:
  /** Capture local data and report whether existing controller rows need refresh. */
  bool captureProps();
  void paint(bool erase);
  loka::app::TextNode *node_;
  Rect rect_;
  Rect paintRect_;
  loka::app::scene::PaintFact<loka::core::String> presented_;
  short textX_;
  short textY_;
  short maxWidth_;
  loka::app::TextWrap wrapMode_;
  loka::app::TextTruncation truncationMode_;
  loka::core::State<loka::core::String> *text_;
};

bool RegisterToolboxTextNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_TOOLBOX_TEXT_CONTEXT_HPP
