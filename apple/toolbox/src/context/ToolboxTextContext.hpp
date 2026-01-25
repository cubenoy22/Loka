#ifndef LOKA_TOOLBOX_TEXT_CONTEXT_HPP
#define LOKA_TOOLBOX_TEXT_CONTEXT_HPP

#include "core2/scene/NativeNodeContext.hpp"
#include "app/Text.hpp"
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

class ToolboxTextContext : public loka::core::scene::NativeNodeContext
{
public:
  explicit ToolboxTextContext(loka::app::TextNode *node);
  virtual ~ToolboxTextContext();

  void setBoundary(loka::core::scene::BoundaryNode *boundary) { boundary_ = boundary; }
  void updateData(loka::core::State<loka::core::String> *text);
  void updateRect(const Rect &rect, short textX, short textY);
  void draw(ToolboxScenePlatformController *controller);
  virtual void render(loka::core::scene::IPlatformController *controller);
  virtual short layout(loka::core::scene::IPlatformController *controller,
                       loka::core::scene::LayoutState &state);

private:
  loka::app::TextNode *node_;
  loka::core::scene::BoundaryNode *boundary_;
  Rect rect_;
  short textX_;
  short textY_;
  loka::core::State<loka::core::String> *text_;
};

#endif // LOKA_TOOLBOX_TEXT_CONTEXT_HPP
