#ifndef LOKA_TOOLBOX_ATTRIBUTED_TEXT_CONTEXT_HPP
#define LOKA_TOOLBOX_ATTRIBUTED_TEXT_CONTEXT_HPP

#include "context/ToolboxProjectedNodeContext.hpp"
#include "context/ToolboxAttributedTextTable.hpp"
#include "context/ToolboxPaintSupport.hpp"
#include "app/nodes/AttributedText.hpp"

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
  namespace testing
  {
    class ToolboxAttributedTextContextAccess;
  }
} // namespace loka

/** Toolbox PER_RUN drawer. Derived geometry and presented history have separate
    validity; only terminal retirement drops the retained projection table. */
class ToolboxAttributedTextContext : public ToolboxProjectedNodeContext
{
public:
  ToolboxAttributedTextContext(loka::app::AttributedTextNode *node, ToolboxScenePlatformController *controller);
  virtual ~ToolboxAttributedTextContext();
  /** Node owns context deletion; keep its allocation gate paired with delete. */
  static void *operator new(std::size_t size) throw();
  static void operator delete(void *storage) throw();
  virtual void onPropsApplied();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous, loka::app::scene::NodeLifecycleFact next);
  virtual loka::app::scene::PaintAnswer queryPaintDamage(const loka::app::scene::PaintQuery &query) const;
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);
  virtual void render(loka::app::scene::IPlatformController *controller);

private:
  friend class loka::testing::ToolboxAttributedTextContextAccess;
  virtual void retireNativeProjection();
  loka::app::AttributedTextNode *node_;
  ToolboxAttributedTextTable table_;
  Rect rect_;
  Rect paintRect_;
  loka::app::scene::PaintFact<loka::app::AttributedString> presented_;
};

bool RegisterToolboxAttributedTextNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);
#endif
