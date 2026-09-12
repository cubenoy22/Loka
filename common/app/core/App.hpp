#ifndef LOKA_APP_HPP
#define LOKA_APP_HPP

#include "app/core/AppComponentGroup.hpp"
#include "app/core/AppComponent.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/MenuController.hpp"
#include "app/Menu.hpp"
#include <cassert>

class Window;
class AppComposition;

namespace loka
{
  namespace app
  {
    namespace testing
    {
      class AppTestAccess;
    }
  }
  namespace dsl
  {
    namespace testing
    {
      class OwnershipDump;
    }
  } // namespace dsl
} // namespace loka

// App is owned by the platform/application layer. Code that needs an App
// instance should reach it through an owner-side path such as Window, not
// through a global current-App accessor.
class App : public AppComponent
{
public:
  explicit App(AppConfigurable *config);
  virtual ~App();

  virtual void run();
  virtual void quit() = 0;
  /** Detaches a Window immediately and queues its silent reclaim for the App clock boundary. */
  void requestWindowClose(Window *window);
  virtual bool handleMenuCommand(int commandId, Window *window);
  loka::app::IdlePolicy idlePolicy() const;
  bool consumeIdle(double elapsedSeconds, double &dispatchElapsedSeconds);
  void handleIdle(double elapsedSeconds);
  bool handleKeyPress(char key);
  void requestMenuInvalidation();
  bool flushMenuInvalidation();
  void invalidateMenu();
  void setDefaultMenuBar(const loka::app::MenuBarDefinition *menuBar);
  const loka::app::MenuBarDefinition *defaultMenuBar() const
  {
    return menuController_.defaultMenuBar();
  }
  void setActiveWindow(Window *window);
  Window *activeWindow() const
  {
    return activeWindow_;
  }

protected:
  /** Drain-internal reclaim step: deletes an already-detached Window. Only
      flushPendingWindowClosures() and subclass observation hooks may call
      this; everything else must go through requestWindowClose(). */
  virtual void windowClosed(Window *window);
  AppComponentGroup *group_;
  bool quitWhenLastWindowClosed_;
  AppConfigurable *config_;
  MenuController menuController_;
  Window *activeWindow_;
  double idleAccumulatedSeconds_;

  const loka::app::MenuBarDefinition *resolveMenuBar(Window *window);
  virtual void applyMenuBar(Window *activeWindow);
  bool refreshDefaultMenuBar();

  const loka::app::MenuCompositionDiff &menuDiff() const
  {
    return menuController_.diff();
  }
  void clearMenuDiff();

  void projectInitialVisibilityChunks();
  /** App clock admission: applies seats before Scene runs; nested window work is refused. */
  void flushWindowInvalidations();
  /** Pre-wait progress: close rows or serviceable Window completion work. */
  bool hasPendingWindowAdmission() const;
  /** Drains one queue snapshot; requests made during the drain wait for the next flush. */
  void flushPendingWindowClosures();

private:
  struct AdmittedWindow;
  bool isWindowClosePending(Window *window) const;
  std::vector<Window *> pendingWindowClosures_;
  bool flushingWindowWork_;

  static void ApplyMenuBarThunk(void *userData, Window *activeWindow);

  friend class loka::dsl::testing::OwnershipDump;
  friend class loka::app::testing::AppTestAccess;
};

#endif // LOKA_APP_HPP
