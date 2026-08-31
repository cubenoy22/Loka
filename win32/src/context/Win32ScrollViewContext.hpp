#ifndef LOKA_WIN32_SCROLL_VIEW_CONTEXT_HPP
#define LOKA_WIN32_SCROLL_VIEW_CONTEXT_HPP

#include <windows.h>

#include "Win32RetirableContext.hpp"
#include "app/nodes/nestable/ScrollView.hpp"

class Win32ScenePlatformController;

/** Owns the child HWND that is both a ScrollView's native viewport and the
    structural clipping parent for the projected subtree. */
class Win32ScrollViewContext : public Win32RetirableContext
{
public:
  Win32ScrollViewContext(Win32ScenePlatformController *controller,
                         HWND parent,
                         int x,
                         int y,
                         int width,
                         int height,
                         loka::app::ScrollViewNode *node);
  virtual ~Win32ScrollViewContext();

  void readLifecycleFactOnAttach();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);

  bool isValid() const;
  HWND hwnd() const;
  void relayout(int x, int y, int width, int height);

  /** Publishes native range/page/position state and returns the clamped
      logical offset represented by that state. */
  int setScrollMetrics(int contentHeight, int viewportHeight, int offset);

private:
  void applyAttachedPresentation();
  void applyDetachedPresentation();
  bool handleVerticalScroll(int command, int thumbPosition);
  bool readScrollInfo(SCROLLINFO &info) const;
  int maximumOffset(const SCROLLINFO &info) const;
  int clampOffset(int value, int maximum) const;
  void setVisualPosition(int value);
  void publishOffset(int value);

  static void EnsureClassRegistered();
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  loka::app::ScrollViewNode *node_;
  HWND hwnd_;
};

#endif // LOKA_WIN32_SCROLL_VIEW_CONTEXT_HPP
