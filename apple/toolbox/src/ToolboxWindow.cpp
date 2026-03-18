#include "ToolboxWindow.hpp"

#include <cstring>
#include <string>
#include <cstdio>
#include <Quickdraw.h>
#include "app/App.hpp"
#include "app/scene/Scene.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "ToolboxWindowContext.hpp"
#include "context/ToolboxNodeContextMapper.hpp"
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

  short WindowTitleBarHeight(WindowPtr window)
  {
    WindowPeek peek = reinterpret_cast<WindowPeek>(window);
    if (!peek || !peek->strucRgn || !peek->contRgn)
    {
      return 0;
    }
    Rect struc = (*peek->strucRgn)->rgnBBox;
    Rect cont = (*peek->contRgn)->rgnBBox;
    short height = static_cast<short>(cont.top - struc.top);
    return height > 0 ? height : 0;
  }
}

ToolboxWindow::ToolboxWindow(PlatformContext *context,
                             const WindowProps &props)
    : Window(context, props), app_(0), window_(0), scenePlatformController_(0), context_(0), needsInvalidate_(false), pendingInvalidateRects_(), titleBarHeight_(0)
{
  window_ = 0;
  context_ = new ToolboxWindowContext(
#if !defined(LOKA_TOOLBOX_CLASSIC_6)
      ToolboxNodeContextMapper::CAP_CONTROL_MANAGER | ToolboxNodeContextMapper::CAP_TEXT_EDIT
#else
      ToolboxNodeContextMapper::CAP_NONE
#endif
  );
  this->titleState().deferBind(&ToolboxWindow::TitleChangedThunk, this);
  this->frameState().deferBind(&ToolboxWindow::FrameChangedThunk, this);
}

ToolboxWindow::~ToolboxWindow()
{
  this->titleState().unbind(&ToolboxWindow::TitleChangedThunk, this);
  this->frameState().unbind(&ToolboxWindow::FrameChangedThunk, this);
  needsInvalidate_ = false;
  pendingInvalidateRects_.clear();
  teardownScene();
  delete context_;
  context_ = 0;
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
  open();
  mountScene();
}

void ToolboxWindow::open()
{
  if (window_)
  {
    return;
  }
  Rect bounds;
  short left = static_cast<short>(this->hasPosition() ? this->positionX() : 50);
  short top = static_cast<short>(this->hasPosition() ? this->positionY() : 50);
  short menuHeight = GetMBarHeight();
  if (menuHeight > 0)
  {
    top = static_cast<short>(top + menuHeight + 1);
  }
  short width = static_cast<short>(this->hasSize() ? this->width() : 300);
  short height = static_cast<short>(this->hasSize() ? this->height() : 300);
  SetRect(&bounds, left, top, static_cast<short>(left + width), static_cast<short>(top + height));

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
  titleBarHeight_ = WindowTitleBarHeight(window_);
  TitleChangedThunk(this);
}

void ToolboxWindow::requestInvalidate()
{
  needsInvalidate_ = true;
  pendingInvalidateRects_.clear();
}

void ToolboxWindow::requestInvalidateRect(const Rect &rect)
{
  if (needsInvalidate_)
  {
    return;
  }
  for (std::size_t i = 0; i < pendingInvalidateRects_.size(); ++i)
  {
    Rect &pending = pendingInvalidateRects_[i];
    if (rect.right < pending.left || rect.left > pending.right ||
        rect.bottom < pending.top || rect.top > pending.bottom)
    {
      continue;
    }
    if (rect.left < pending.left)
    {
      pending.left = rect.left;
    }
    if (rect.top < pending.top)
    {
      pending.top = rect.top;
    }
    if (rect.right > pending.right)
    {
      pending.right = rect.right;
    }
    if (rect.bottom > pending.bottom)
    {
      pending.bottom = rect.bottom;
    }
    return;
  }
  pendingInvalidateRects_.push_back(rect);
}

void ToolboxWindow::flushInvalidate()
{
  if (!window_)
  {
    return;
  }
  if (!needsInvalidate_ && pendingInvalidateRects_.empty())
  {
    return;
  }
  if (needsInvalidate_)
  {
    needsInvalidate_ = false;
    pendingInvalidateRects_.clear();
    this->draw();
    return;
  }
  needsInvalidate_ = false;
  std::vector<Rect> rects = pendingInvalidateRects_;
  pendingInvalidateRects_.clear();
  for (std::size_t i = 0; i < rects.size(); ++i)
  {
    this->drawDirty(rects[i]);
  }
}

bool ToolboxWindow::hasPendingInvalidate() const
{
  return needsInvalidate_ || !pendingInvalidateRects_.empty();
}

void ToolboxWindow::refreshFrame()
{
  FrameChangedThunk(this);
}

void ToolboxWindow::FrameChangedThunk(void *userData)
{
  ToolboxWindow *self = static_cast<ToolboxWindow *>(userData);
  if (!self || !self->window_)
  {
    return;
  }
  loka::core::Frame frame = self->frameState().get();
  if (frame.hasPosition())
  {
    short y = static_cast<short>(frame.y >= 0 ? frame.y : 0);
    short menuHeight = GetMBarHeight();
    if (menuHeight > 0)
    {
      y = static_cast<short>(y + menuHeight);
    }
    if (self->titleBarHeight_ > 0)
    {
      y = static_cast<short>(y + self->titleBarHeight_);
    }
    MoveWindow(self->window_, static_cast<short>(frame.x), y, false);
  }
  if (frame.hasSize())
  {
    SizeWindow(self->window_, static_cast<short>(frame.width), static_cast<short>(frame.height), true);
  }
}

void ToolboxWindow::TitleChangedThunk(void *userData)
{
  ToolboxWindow *self = static_cast<ToolboxWindow *>(userData);
  if (!self || !self->window_)
  {
    return;
  }
  loka::core::String titleValue = self->titleState().get();
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
  SetWTitle(self->window_, titleStr);
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
    scenePlatformController_->flushRetiredNativeHandles();
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
  scenePlatformController_->renderDirty(rect);
  Rect bounds = window_->portRect;
  ClipRect(&bounds);
  SetPort(oldPort);
}

void ToolboxWindow::invalidateWindow()
{
  teardownScene();
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

void ToolboxWindow::synchronizeScenePlatform()
{
  if (scenePlatformController_)
  {
    scenePlatformController_->synchronize();
  }
}

bool ToolboxWindow::hasPendingScenePlatformSync() const
{
  return scenePlatformController_ ? scenePlatformController_->hasPendingSync() : false;
}

void ToolboxWindow::mountScene()
{
  loka::app::scene::Scene *currentScene = this->scene();
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
    loka::app::scene::Scene *currentScene = this->scene();
    if (currentScene)
    {
      currentScene->unmount();
    }
    delete scenePlatformController_;
    scenePlatformController_ = 0;
  }
}
