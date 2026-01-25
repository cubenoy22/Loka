#ifndef LOKA_TOOLBOX_CELL_CONTEXT_HPP
#define LOKA_TOOLBOX_CELL_CONTEXT_HPP

#include "core2/scene/NativeNodeContext.hpp"
#include "app/Cell.hpp"
#include <Quickdraw.h>

class ToolboxScenePlatformController;

namespace loka
{
  namespace core
  {
    template <typename T>
    class State;
    namespace scene
    {
      class BoundaryNode;
    }
  }
}

class ToolboxCellContext : public loka::core::scene::NativeNodeContext
{
public:
  explicit ToolboxCellContext(loka::app::CellNode *node);
  virtual ~ToolboxCellContext();

  void setBoundary(loka::core::scene::BoundaryNode *boundary) { boundary_ = boundary; }
  void updateData(loka::core::State<loka::core::String> *text);
  void updateRect(const Rect &rect);
  void draw(ToolboxScenePlatformController *controller);
  virtual void render(loka::core::scene::IPlatformController *controller);
  virtual short layout(loka::core::scene::IPlatformController *controller,
                       loka::core::scene::LayoutState &state);

private:
  loka::app::CellNode *node_;
  loka::core::scene::BoundaryNode *boundary_;
  Rect rect_;
  loka::core::State<loka::core::String> *text_;
};

#endif // LOKA_TOOLBOX_CELL_CONTEXT_HPP
