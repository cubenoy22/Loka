#ifndef LOKA_TOOLBOX_CELL_CONTEXT_HPP
#define LOKA_TOOLBOX_CELL_CONTEXT_HPP

#include "core2/scene/NativeNodeContext.hpp"
#include "app/Cell.hpp"
#include <Quickdraw.h>

class ToolboxScenePlatformController;

namespace declara
{
  namespace core
  {
    template <typename T>
    class State;
  }
}

class ToolboxCellContext : public declara::core::scene::NativeNodeContext
{
public:
  explicit ToolboxCellContext(declara::app::CellNode *node);
  virtual ~ToolboxCellContext();

  void updateData(declara::core::State<loka::core::String> *text);
  void updateRect(const Rect &rect);
  void draw(ToolboxScenePlatformController *controller);
  virtual void render(declara::core::scene::IPlatformController *controller);
  virtual short layout(declara::core::scene::IPlatformController *controller,
                       declara::core::scene::LayoutState &state);

private:
  declara::app::CellNode *node_;
  Rect rect_;
  declara::core::State<loka::core::String> *text_;
};

#endif // LOKA_TOOLBOX_CELL_CONTEXT_HPP
