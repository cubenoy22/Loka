#include "ToolboxWindow.hpp"

#include <cstring>
#include <string>
#include <Quickdraw.h>
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
  SetRect(&bounds, 60, 60, 420, 320);

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

bool ToolboxWindow::handleMouseDown(const Point &globalPoint)
{
  if (!window_ || !scenePlatformController_)
  {
    return false;
  }
  GrafPtr oldPort;
  GetPort(&oldPort);
  SetPort(window_);
  Point localPoint = globalPoint;
  GlobalToLocal(&localPoint);
  bool handled = scenePlatformController_->handleMouseDown(localPoint);
  SetPort(oldPort);
  return handled;
}

bool ToolboxWindow::handleKeyDown(char key)
{
  if (!scenePlatformController_)
  {
    return false;
  }
  return scenePlatformController_->handleKeyDown(key);
}

void ToolboxWindow::idleControls()
{
  if (scenePlatformController_)
  {
    scenePlatformController_->idleTextEdits();
  }
}

void ToolboxWindow::updateCursor()
{
  if (!window_ || !scenePlatformController_)
  {
    return;
  }
  GrafPtr oldPort;
  GetPort(&oldPort);
  SetPort(window_);
  Point localPoint;
  GetMouse(&localPoint);
  bool inEdit = scenePlatformController_->isPointInEdit(localPoint);
  SetPort(oldPort);
  if (inEdit)
  {
    CursHandle ibeam = GetCursor(iBeamCursor);
    if (ibeam)
    {
      SetCursor(*ibeam);
    }
  }
  else
  {
    InitCursor();
  }
}

void ToolboxWindow::drawDirty(const Rect &rect)
{
  if (!window_ || !scenePlatformController_)
  {
    return;
  }
  GrafPtr oldPort;
  GetPort(&oldPort);
  SetPort(window_);
  Rect clip = rect;
  ClipRect(&clip);
  EraseRect(&rect);
  scenePlatformController_->render();
  scenePlatformController_->drawControlsInRect(rect);
  Rect bounds = window_->portRect;
  ClipRect(&bounds);
  SetPort(oldPort);
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
    scenePlatformController_->drawControlsInRect(window_->portRect);
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
