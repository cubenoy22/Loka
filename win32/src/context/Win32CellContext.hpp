#ifndef LOKA_WIN32_CELL_CONTEXT_HPP
#define LOKA_WIN32_CELL_CONTEXT_HPP

#include <windows.h>
#include "app/scene/projection/NativeNodeContext.hpp"
#include "core/String.hpp"
#include <string>

namespace loka
{
  namespace core
  {
    template <typename T> class State;
  }
} // namespace loka

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

class Win32CellContext : public loka::app::scene::NativeNodeContext
{
public:
  Win32CellContext(HWND parent, int x, int y, int width, int height, loka::app::CellNode *node);
  virtual ~Win32CellContext();
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);
  virtual void onNodeAttached();
  virtual void onNodeDetached();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);
  void relayout(int x, int y, int width, int height);

private:
  void applyAttachedPresentation();
  void applyDetachedPresentation();
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static void EnsureClassRegistered();
  void bindText();
  void unbindText();
  void applyText();
  void drawCell(HDC hdc, const RECT &rect);
  static void TextChangedThunk(void *userData);

  loka::app::CellNode *node_;
  HWND hwnd_;
  loka::core::State<loka::core::String> *textState_;
  std::string text_;
};

void RegisterWin32CellNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_WIN32_CELL_CONTEXT_HPP
