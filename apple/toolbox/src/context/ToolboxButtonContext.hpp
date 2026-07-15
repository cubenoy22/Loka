#ifndef LOKA_TOOLBOX_BUTTON_CONTEXT_HPP
#define LOKA_TOOLBOX_BUTTON_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"
#include "app/nodes/controls/Button.hpp"
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

class ToolboxButtonContext : public loka::app::scene::NativeNodeContext
{
public:
  ToolboxButtonContext(loka::app::ButtonNode *node, ToolboxScenePlatformController *controller);
  virtual ~ToolboxButtonContext();

  void setBoundary(loka::app::scene::BoundaryNode *boundary)
  {
    boundary_ = boundary;
  }
  void updateData(const loka::core::String &label,
                  loka::core::EmitterState *emitter,
                  loka::core::State<bool> *enabled,
                  short resourceId,
                  int controlTag);
  void updateRect(const Rect &rect);
  void draw(ToolboxScenePlatformController *controller);
  virtual void render(loka::app::scene::IPlatformController *controller);
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);
  bool handleMouseDown(const Point &point, ToolboxScenePlatformController *controller);

private:
  loka::app::ButtonNode *node_;
  loka::app::scene::BoundaryNode *boundary_;
  Rect rect_;
  loka::core::String label_;
  loka::core::EmitterState *emitter_;
  loka::core::State<bool> *enabled_;
  short resourceId_;
  ToolboxScenePlatformController *controller_;
};

#endif // LOKA_TOOLBOX_BUTTON_CONTEXT_HPP
