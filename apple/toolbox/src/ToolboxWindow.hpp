#ifndef LOKA_TOOLBOX_WINDOW_HPP
#define LOKA_TOOLBOX_WINDOW_HPP

#include "app/core/Window.hpp"
#include "ToolboxActivationPhase.hpp"
#include <Windows.h>
#include <vector>

class App;
class ToolboxScenePlatformController;
class ToolboxWindowContext;
typedef void (*DeferredDumpCompletion)(void *userData);

class ToolboxWindow : public Window
{
public:
  ToolboxWindow(PlatformContext *context, const WindowProps &props);
  virtual ~ToolboxWindow();
  virtual ToolboxWindow *asToolboxWindow()
  {
    return this;
  }

  void setApp(App *app);
  void ensureSceneMounted();
  void open();
  void dispatchDeferredDebugDumpCompletion();
  void requestInvalidate();
  void requestInvalidateWithReason(const char *reason);
  void requestInvalidateRect(const Rect &rect);
  void flushInvalidate();
  bool hasPendingInvalidate() const;
  bool handleMouseDown(const Point &globalPoint);
  /** Tracks a grow-box drag from the given global mouse-down point, resizes
      the native window, and publishes the new content size (GH #524). */
  void handleGrow(const Point &globalPoint);
  /** Invalidates the grow-box corner so the next update redraws it in the
      window's current hilite state. */
  void invalidateGrowIcon();
  bool handleKeyDown(char key);
  void drawDirty(const Rect &rect);
  void idleControls(ActivationPhase phase);
  void updateCursor();
  void invalidateWindow();
  void draw();
  void refreshFrame();
  virtual bool hasPendingScenePlatformSync() const;
  virtual void synchronizeScenePlatform();
  virtual void drainNativeRetirements();
  virtual bool dumpDebugStatsToTimestampedFile();
  virtual void resetDebugStats();
  virtual void requestDeferredDebugDump();
  virtual void requestDeferredDebugDumpWithCompletion(DeferredDumpCompletion completion, void *userData);
  virtual void flushDeferredDebugDump();
  virtual bool queryDisplayScalePercent(int &out) const;
  virtual bool queryDisplayDepth(int &out) const;
  // queryDisplayAppearance is deliberately not overridden: Classic has no way
  // to report light versus dark. The Appearance Manager and extensions such as
  // Kaleidoscope patch the standard definition procedures, so themes apply to
  // this backend already -- it draws through the standard CDEF and bakes no
  // colours -- but that is inheritance, not a fact anything can query.
  WindowPtr window() const
  {
    return window_;
  }
  ToolboxWindowContext *context() const
  {
    return context_;
  }
  ToolboxScenePlatformController *scenePlatformController() const
  {
    return scenePlatformController_;
  }

private:
  static void TitleChangedThunk(void *userData);
  static void FrameChangedThunk(void *userData);
  App *app_;
  WindowPtr window_;
  ToolboxScenePlatformController *scenePlatformController_;
  ToolboxWindowContext *context_;
  bool needsInvalidate_;
  bool pendingDebugDump_;
  DeferredDumpCompletion pendingDebugDumpCompletion_;
  void *pendingDebugDumpUserData_;
  DeferredDumpCompletion pendingDeferredDebugDumpCompletion_;
  void *pendingDeferredDebugDumpUserData_;
  int pendingDeferredDebugDumpCompletionDelay_;
  std::vector<Rect> pendingInvalidateRects_;
  short titleBarHeight_;

  void mountScene();
  void teardownScene();
};

#endif // LOKA_TOOLBOX_WINDOW_HPP
