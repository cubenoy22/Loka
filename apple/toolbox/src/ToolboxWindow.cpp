#include "ToolboxWindow.hpp"

#include <cstring>
#include <string>
#include "loka/core/String.hpp"
#include "loka/platform/StringUTF8.hpp"

namespace
{
  void CopyToPascalString(const std::string &value, Str255 out)
  {
    std::size_t length = value.size();
    if (length > 255)
      length = 255;
    out[0] = static_cast<unsigned char>(length);
    if (length > 0)
      std::memcpy(out + 1, value.data(), length);
  }
}

ToolboxWindow::ToolboxWindow(PlatformContext *context,
                             const WindowProps &props)
    : Window(context, props), app_(0), window_(0)
{
  Rect bounds;
  SetRect(&bounds, 60, 60, 420, 300);

  loka::core::String titleValue = this->titleState().get();
  if (titleValue.empty())
  {
    titleValue = loka::core::String::Literal("Loka");
  }
  std::string title;
  if (!loka::platform::CollectUtf8(titleValue, title))
  {
    title = "Loka";
  }
  Str255 titleStr;
  CopyToPascalString(title, titleStr);

  window_ = NewWindow(0, &bounds, titleStr, true, documentProc, (WindowPtr)-1, true, 0);
}

ToolboxWindow::~ToolboxWindow()
{
  if (window_)
  {
    DisposeWindow(window_);
    window_ = 0;
  }
}

void ToolboxWindow::setApp(App *app)
{
  app_ = app;
}

void ToolboxWindow::invalidateWindow()
{
  window_ = 0;
}

void ToolboxWindow::draw()
{
  if (!window_)
    return;

  GrafPtr oldPort;
  GetPort(&oldPort);
  SetPort(window_);

  EraseRect(&window_->portRect);
  // TODO: render scene content here

  SetPort(oldPort);
}
