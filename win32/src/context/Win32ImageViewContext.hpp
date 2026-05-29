#ifndef LOKA_WIN32_IMAGE_VIEW_CONTEXT_HPP
#define LOKA_WIN32_IMAGE_VIEW_CONTEXT_HPP

#include <windows.h>
#include "app/nodes/ImageView.hpp"
#include "core/State.hpp"
#include "core/resource/Image.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class PlatformNodeHandlerRegistry;
    }
  }
}

class Win32ImageViewContext : public loka::app::scene::NodeContext
{
public:
  Win32ImageViewContext(HWND parent, int x, int y, int width, int height, loka::app::ImageViewNode *node);
  virtual ~Win32ImageViewContext();
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);
  virtual void onNodeAttached();
  virtual void onNodeDetached();

  static void EnsureClassRegistered();
  void relayout(int x, int y, int width, int height);

private:
  void bindImage();
  void unbindImage();
  void applyImage();
  void drawImage(HDC hdc, const RECT &rect);

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static void ImageChangedThunk(void *userData);

  loka::app::ImageViewNode *node_;
  HWND hwnd_;
  loka::core::State<loka::core::resource::Image> *imageState_;
  loka::core::resource::Image image_;
};

void RegisterWin32ImageViewNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_WIN32_IMAGE_VIEW_CONTEXT_HPP
