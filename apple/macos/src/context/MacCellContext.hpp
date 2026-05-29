#ifndef LOKA_MAC_CELL_CONTEXT_HPP
#define LOKA_MAC_CELL_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"
#include "core/State.hpp"
#include "core/String.hpp"

namespace loka
{
  namespace app
  {
    class CellNode;
    namespace scene
    {
      class PlatformNodeHandlerRegistry;
    }
  } // namespace app
} // namespace loka

class MacCellContext : public loka::app::scene::NativeNodeContext
{
public:
  MacCellContext(void *parentView, int x, int y, int width, int height, loka::app::CellNode *node);
  virtual ~MacCellContext();
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);
  virtual void onNodeAttached();
  virtual void onNodeDetached();
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
  bool textStateBound_;
};

void RegisterMacCellNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_MAC_CELL_CONTEXT_HPP
