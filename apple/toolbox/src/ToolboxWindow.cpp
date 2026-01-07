#include "ToolboxWindow.hpp"

#include <cstring>
#include <string>
#include "core/App.hpp"
#include "core2/scene/Scene.hpp"
#include "ToolboxScenePlatformController.hpp"
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
    : Window(context, props), app_(0), window_(0), scenePlatformController_(0), needsInvalidate_(false)
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
  teardownScene();
  if (window_)
  {
    DisposeWindow(window_);
    window_ = 0;
  }
}

void ToolboxWindow::setApp(App *app)
{
  app_ = app;
  if (app_)
  {
    app_->setActiveWindow(this);
  }
}

void ToolboxWindow::ensureSceneMounted()
{
  mountScene();
}

void ToolboxWindow::requestInvalidate()
{
  needsInvalidate_ = true;
}

void ToolboxWindow::flushInvalidate()
{
  if (!needsInvalidate_ || !window_)
  {
    return;
  }
  needsInvalidate_ = false;
  InvalRect(&window_->portRect);
}

void ToolboxWindow::invalidateWindow()
{
  teardownScene();
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
  if (scenePlatformController_)
  {
    scenePlatformController_->render();
  }

  SetPort(oldPort);
}

void ToolboxWindow::mountScene()
{
  declara::core::scene::Scene *currentScene = this->scene();
  if (!currentScene || scenePlatformController_ || !window_)
  {
    return;
  }
  scenePlatformController_ = new ToolboxScenePlatformController(this);
  currentScene->mount(scenePlatformController_);
}

void ToolboxWindow::teardownScene()
{
  if (scenePlatformController_)
  {
    declara::core::scene::Scene *currentScene = this->scene();
    if (currentScene)
    {
      currentScene->unmount();
    }
    delete scenePlatformController_;
    scenePlatformController_ = 0;
  }
}
