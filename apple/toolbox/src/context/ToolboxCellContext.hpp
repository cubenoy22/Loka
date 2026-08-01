#ifndef LOKA_TOOLBOX_CELL_CONTEXT_HPP
#define LOKA_TOOLBOX_CELL_CONTEXT_HPP

#include "context/ToolboxProjectedNodeContext.hpp"
#include "app/nodes/controls/Cell.hpp"
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
    template <typename T> class State;
    namespace scene
    {
      class BoundaryNode;
    }
  } // namespace core
} // namespace loka

class ToolboxCellContext : public ToolboxProjectedNodeContext
{
public:
  explicit ToolboxCellContext(loka::app::CellNode *node);
  virtual ~ToolboxCellContext();

  void updateData(loka::core::State<loka::core::String> *text);
  void updateRect(const Rect &rect);
  void draw(ToolboxScenePlatformController *controller);
  virtual void render(loka::app::scene::IPlatformController *controller);
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);
  bool handleMouseDown(const Point &point, ToolboxScenePlatformController *controller);

private:
  loka::app::CellNode *node_;
  Rect rect_;
  loka::core::State<loka::core::String> *text_;
};

bool RegisterToolboxCellNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_TOOLBOX_CELL_CONTEXT_HPP
