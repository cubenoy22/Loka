#ifndef LOKA_MAC_CELL_CONTEXT_HPP
#define LOKA_MAC_CELL_CONTEXT_HPP

#include "app/scene/NativeNodeContext.hpp"
#include "loka/core/State.hpp"
#include "loka/core/String.hpp"

namespace loka
{
  namespace app
  {
    class CellNode;
    namespace scene
    {
      class PlatformNodeHandlerRegistry;
    }
  }
}

class MacCellContext : public loka::app::scene::NativeNodeContext
{
public:
  MacCellContext(void *parentView, int x, int y, int width, int height, loka::app::CellNode *node);
  virtual ~MacCellContext();
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);
  void handleClick();
  void relayout(int x, int y, int width, int height);

private:
  void bindText();
  void unbindText();
  void applyText();
  static void TextChangedThunk(void *userData);

  loka::app::CellNode *node_;
  void *view_;
  loka::core::State<loka::core::String> *textState_;
};

void RegisterMacCellNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_MAC_CELL_CONTEXT_HPP
