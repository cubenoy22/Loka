#ifndef LOKA_MAC_APP_HPP
#define LOKA_MAC_APP_HPP

#include "app/App.hpp"
#include <mach/mach_time.h>
#include <vector>

class MacWindow;

class MacApp : public App
{
public:
  explicit MacApp(AppConfigurable *config);
  virtual ~MacApp();

  virtual void run();
  virtual void quit();
  void handleMenuCommand(int commandId);
  void flushInvalidationsTick();

  struct MenuCommand
  {
    int commandId;
    loka::app::MenuActionType action;
    loka::core::EmitterState *emitter;
  };

  struct MenuBinding
  {
    void *menuItem;
    loka::core::State<bool> *enabledState;
    bool invertEnabled;
  };

protected:
  virtual void applyMenuBar(Window *activeWindow);

private:
  void clearMenuBindings();
  void startInvalidationFlushTimer();
  void stopInvalidationFlushTimer();
  int nextCommandId_;
  std::vector<MenuCommand> commands_;
  std::vector<MenuBinding *> bindings_;
  void *menuTarget_;
  void *flushTarget_;
  void *flushTimer_;
  unsigned long long lastIdleTick_;
  mach_timebase_info_data_t idleTimebase_;
};

#endif // LOKA_MAC_APP_HPP
