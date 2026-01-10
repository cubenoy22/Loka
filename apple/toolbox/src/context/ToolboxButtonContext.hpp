#ifndef LOKA_TOOLBOX_BUTTON_CONTEXT_HPP
#define LOKA_TOOLBOX_BUTTON_CONTEXT_HPP

#include "core2/scene/NativeNodeContext.hpp"
#include "app/Button.hpp"
#include "loka/core/String.hpp"
#include <Quickdraw.h>

class ToolboxScenePlatformController;
namespace declara
{
  namespace core
  {
    namespace scene
    {
      class BoundaryNode;
    }
  }
}
namespace declara
{
  namespace core
  {
    namespace scene
    {
      class IPlatformController;
    }
  }
}

class ToolboxButtonContext : public declara::core::scene::NativeNodeContext
{
public:
  explicit ToolboxButtonContext(declara::app::ButtonNode *node);
  virtual ~ToolboxButtonContext();

  void setBoundary(declara::core::scene::BoundaryNode *boundary) { boundary_ = boundary; }
  void updateData(const loka::core::String &label,
                  declara::core::EmitterState *emitter,
                  declara::core::State<bool> *enabled,
                  short resourceId);
  void updateRect(const Rect &rect);
  void draw(ToolboxScenePlatformController *controller);
  virtual void render(declara::core::scene::IPlatformController *controller);
  virtual short layout(declara::core::scene::IPlatformController *controller,
                       declara::core::scene::LayoutState &state);

private:
  declara::app::ButtonNode *node_;
  declara::core::scene::BoundaryNode *boundary_;
  Rect rect_;
  loka::core::String label_;
  declara::core::EmitterState *emitter_;
  declara::core::State<bool> *enabled_;
  short resourceId_;
};

#endif // LOKA_TOOLBOX_BUTTON_CONTEXT_HPP
