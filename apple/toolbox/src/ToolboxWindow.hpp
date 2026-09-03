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
      the native window, and publishes the new content frame (GH #524). */
  void handleGrow(const Point &globalPoint);
  /** Publishes the current native content frame. */
  void storeCurrentNativeContentFrame();
  /** Invalidates the grow-box corner so the next update redraws it in the
      window's current hilite state. */
  void invalidateGrowIcon();
  bool handleKeyDown(char key);
  void drawDirty(const Rect &rect);
  void idleControls(ActivationPhase phase);
  void updateCursor();
  void invalidateWindow();
  void draw();
  /** Preserves the native content position after a menu-bar rebuild without
      reapplying the application's declared frame intent. */
  void preserveNativeContentPositionAfterMenuBarChange();
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
  // A full request stays pending while draw() runs, so its bool alone cannot
  // distinguish covered pre-flush rectangles from later-flush retry work.
  enum InvalidatePaintPhase
  {
    INVALIDATE_PAINT_IDLE = 0,
    INVALIDATE_PAINT_DRAWING
  };
  // Both rectangle queues are reserved once at construction and never grow
  // afterwards: a refused paint requests its retry from inside draw(), which
  // is exactly when the application heap may be exhausted, and a vector
  // allocation there would abort to the Finder instead of retrying later.
  enum
  {
    kPendingInvalidateRectCapacity = 16
  };

  static void TitleChangedThunk(void *userData);
  static void FrameChangedThunk(void *userData);
  App *app_;
  WindowPtr window_;
  ToolboxScenePlatformController *scenePlatformController_;
  ToolboxWindowContext *context_;
  bool needsInvalidate_;
  InvalidatePaintPhase invalidatePaintPhase_;
  bool pendingDebugDump_;
  DeferredDumpCompletion pendingDebugDumpCompletion_;
  void *pendingDebugDumpUserData_;
  DeferredDumpCompletion pendingDeferredDebugDumpCompletion_;
  void *pendingDeferredDebugDumpUserData_;
  int pendingDeferredDebugDumpCompletionDelay_;
  std::vector<Rect> pendingInvalidateRects_;
  std::vector<Rect> flushingInvalidateRects_;
  short titleBarHeight_;

  void mountScene();
  void teardownScene();
  loka::core::Frame nativeContentFrame() const;
  void drawGrowBox();
};

#endif // LOKA_TOOLBOX_WINDOW_HPP
