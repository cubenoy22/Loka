#ifndef LOKA_TOOLBOX_WINDOW_HPP
#define LOKA_TOOLBOX_WINDOW_HPP

#include "app/Window.hpp"
#include <Windows.h>
#include <vector>

class App;
class ToolboxScenePlatformController;
class ToolboxWindowContext;

class ToolboxWindow : public Window
{
public:
  ToolboxWindow(PlatformContext *context, const WindowProps &props);
  virtual ~ToolboxWindow();
  virtual ToolboxWindow *asToolboxWindow() { return this; }

  void setApp(App *app);
  void ensureSceneMounted();
  void open();
  void requestInvalidate();
  void requestInvalidateRect(const Rect &rect);
  void flushInvalidate();
  bool hasPendingInvalidate() const;
  bool handleMouseDown(const Point &globalPoint);
  bool handleKeyDown(char key);
  void drawDirty(const Rect &rect);
  void idleControls();
  void updateCursor();
  void invalidateWindow();
  void draw();
  void refreshFrame();
  virtual bool hasPendingScenePlatformSync() const;
  virtual void synchronizeScenePlatform();
  WindowPtr window() const { return window_; }
  ToolboxWindowContext *context() const { return context_; }

private:
  static void TitleChangedThunk(void *userData);
  static void FrameChangedThunk(void *userData);
  App *app_;
  WindowPtr window_;
  ToolboxScenePlatformController *scenePlatformController_;
  ToolboxWindowContext *context_;
  bool needsInvalidate_;
  std::vector<Rect> pendingInvalidateRects_;
  short titleBarHeight_;

  void mountScene();
  void teardownScene();
};

#endif // LOKA_TOOLBOX_WINDOW_HPP
