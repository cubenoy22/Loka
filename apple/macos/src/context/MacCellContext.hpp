#ifndef LOKA_MAC_CELL_CONTEXT_HPP
#define LOKA_MAC_CELL_CONTEXT_HPP

#include "core2/scene/NativeNodeContext.hpp"
#include "core/State.hpp"
#include "loka/core/String.hpp"

namespace declara
{
  namespace app
  {
    class CellNode;
  }
}

class MacCellContext : public declara::core::scene::NativeNodeContext
{
public:
  MacCellContext(void *parentView, int x, int y, int width, int height, declara::app::CellNode *node);
  virtual ~MacCellContext();
  void handleClick();

private:
  void bindText();
  void unbindText();
  void applyText();
  static void TextChangedThunk(void *userData);

  declara::app::CellNode *node_;
  void *view_;
  State<loka::core::String> *textState_;
};

#endif // LOKA_MAC_CELL_CONTEXT_HPP
