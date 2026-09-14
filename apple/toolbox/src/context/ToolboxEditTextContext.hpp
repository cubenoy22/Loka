#ifndef LOKA_TOOLBOX_EDIT_TEXT_CONTEXT_HPP
#define LOKA_TOOLBOX_EDIT_TEXT_CONTEXT_HPP

#include "context/ToolboxProjectedNodeContext.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "core/String.hpp"
#include "app/scene/projection/PaintFact.hpp"
#include <Quickdraw.h>
#include <TextEdit.h>

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
  ToolboxEditTextContext(loka::app::EditTextNode *node, ToolboxScenePlatformController *controller);
  virtual ~ToolboxEditTextContext();
  virtual void onPropsApplied();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);
  virtual loka::app::scene::PaintAnswer queryPaintDamage(const loka::app::scene::PaintQuery &query) const;

  void updateData(loka::core::State<loka::core::String> *text);
  void updateRect(const Rect &outerRect, const Rect &textRect, short textX, short textY);
  void draw(ToolboxScenePlatformController *controller);
  /** Replay established native placement without creating or reprojecting TE. */
  void repaint(TEHandle te);
  /** The rect draw() frames. The retained binding carries the inset text rect,
      which TEUpdate needs, so a dirty replay gated on that one would skip a
      region covering only the chrome the frame lands on. */
  const Rect &chromeRect() const
  {
    return rect_;
  }
  virtual loka::core::State<loka::core::String> *projectedTextState()
  {
    return text_;
  }
  virtual void render(loka::app::scene::IPlatformController *controller);
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);

private:
  friend class ToolboxScenePlatformController;
  /** Native retirement revokes the proof even if this context stays attached. */
  void invalidateNativePresentation() { this->presented_.invalidate(); }
  /** Known only after full TE presentation, and while its binding is installed. */
  loka::app::scene::PaintFact<loka::core::String> presented_;
  /** Capture local data and report whether existing controller rows need refresh. */
  bool captureProps();
  loka::app::EditTextNode *node_;
  Rect rect_;
  Rect paintRect_;
  Rect textRect_;
  short textX_;
  short textY_;
  loka::core::State<loka::core::String> *text_;
};

bool RegisterToolboxEditTextNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_TOOLBOX_EDIT_TEXT_CONTEXT_HPP
