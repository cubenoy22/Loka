#ifndef LOKA_TOOLBOX_CELL_CONTEXT_HPP
#define LOKA_TOOLBOX_CELL_CONTEXT_HPP

#include "app/scene/NativeNodeContext.hpp"
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

class ToolboxCellContext : public loka::app::scene::NativeNodeContext
{
public:
  explicit ToolboxCellContext(loka::app::CellNode *node);
  virtual ~ToolboxCellContext();

  void setBoundary(loka::app::scene::BoundaryNode *boundary) { boundary_ = boundary; }
  void updateData(loka::core::State<loka::core::String> *text);
  void updateRect(const Rect &rect);
  void draw(ToolboxScenePlatformController *controller);
  virtual void render(loka::app::scene::IPlatformController *controller);
  virtual short layout(loka::app::scene::IPlatformController *controller,
                       loka::app::scene::LayoutState &state);

private:
  loka::app::CellNode *node_;
  loka::app::scene::BoundaryNode *boundary_;
  Rect rect_;
  loka::core::State<loka::core::String> *text_;
};

#endif // LOKA_TOOLBOX_CELL_CONTEXT_HPP
