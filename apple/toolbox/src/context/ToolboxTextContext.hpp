#ifndef LOKA_TOOLBOX_TEXT_CONTEXT_HPP
#define LOKA_TOOLBOX_TEXT_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"
#include "app/nodes/Text.hpp"
#include "loka/core/String.hpp"
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

class ToolboxTextContext : public loka::app::scene::NativeNodeContext
{
public:
  explicit ToolboxTextContext(loka::app::TextNode *node);
  virtual ~ToolboxTextContext();

  void setBoundary(loka::app::scene::BoundaryNode *boundary) { boundary_ = boundary; }
  void updateData(loka::core::State<loka::core::String> *text);
  void updateRect(const Rect &rect, short textX, short textY);
  short visibleWidth() const;
  void draw(ToolboxScenePlatformController *controller);
  virtual void render(loka::app::scene::IPlatformController *controller);
  virtual short layout(loka::app::scene::IPlatformController *controller,
                       loka::app::scene::LayoutState &state);

private:
  loka::app::TextNode *node_;
  loka::app::scene::BoundaryNode *boundary_;
  Rect rect_;
  short textX_;
  short textY_;
  short maxWidth_;
  loka::app::TextWrap wrapMode_;
  loka::app::TextTruncation truncationMode_;
  loka::core::State<loka::core::String> *text_;
};

#endif // LOKA_TOOLBOX_TEXT_CONTEXT_HPP
