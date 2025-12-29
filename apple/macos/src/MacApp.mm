#include "MacApp.hpp"
#include "MacWindow.hpp"
#include <AppKit/AppKit.h>
#include "core/AppComponent.hpp"

MacApp::MacApp(AppConfigurable *config)
    : App(config)
{
}

MacApp::~MacApp()
{
}

void MacApp::run()
{
  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

  App::run();

  if (group_)
  {
    const std::vector<AppComponent *> &comps = group_->getComponents();
    for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
    {
      MacWindow *macWin = dynamic_cast<MacWindow *>(*it);
      if (macWin)
      {
        macWin->setApp(this);
      }
    }
  }

  [NSApp activateIgnoringOtherApps:YES];
  [NSApp run];
}

void MacApp::quit()
{
  [NSApp terminate:nil];
}
