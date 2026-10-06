#ifndef LOKA_WIN32APP_HPP
#define LOKA_WIN32APP_HPP

#include "app/core/App.hpp"
#include "core/Operation.hpp"
#include <windows.h>
#include <vector>

class Win32Window;

class Win32App : public App
{
protected:
  Win32App(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow);
  virtual ~Win32App();
  friend class Win32PlatformContext;

public:
  virtual void run();
  virtual void quit();
  /** Continuation for an already dispatched REBUILD_MENU action. */
  bool handleMenuCommand(int commandId, Window *window);

protected:
  /** Routes one non-quit message: menu shortcut, dialog navigation, dispatch. */
  static void TranslateOrDispatch(MSG &msg);
  virtual void projectMenu(Window *window, const loka::app::MenuBarDefinition *bar,
                           const loka::app::scene::Scene *source);

private:
  /** Flushes Scene work produced inside the focus completion's operation in
      the same iteration with the second window flush. The Null model is
      WindowAdmissionTestApp::operationLoop(). */
  void flushIterationTail(loka::core::Operation &turn);
  HINSTANCE hInstance_;
  int nCmdShow_;
};

#endif // LOKA_WIN32APP_HPP
