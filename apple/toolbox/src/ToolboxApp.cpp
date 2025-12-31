#include "ToolboxApp.hpp"
#include "ToolboxWindow.hpp"

#include <Dialogs.h>
#include <Events.h>
#include <Fonts.h>
#include <Menus.h>
#include <Quickdraw.h>
#include <TextEdit.h>
#include <Windows.h>

ToolboxApp::ToolboxApp(AppConfigurable *config) : App(config), running_(false) {}
ToolboxApp::~ToolboxApp() {}

void ToolboxApp::run()
{
  InitGraf(&qd.thePort);
  InitFonts();
  InitWindows();
  InitMenus();
  TEInit();
  InitDialogs(0);
  InitCursor();

  App::run();
  if (group_)
  {
    const std::vector<AppComponent *> &comps = group_->getComponents();
    for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
    {
      ToolboxWindow *toolboxWindow = dynamic_cast<ToolboxWindow *>(*it);
      if (toolboxWindow)
      {
        toolboxWindow->setApp(this);
      }
    }
  }
  running_ = true;
  while (running_)
  {
    EventRecord event;
    WaitNextEvent(everyEvent, &event, 1, 0);
    if (event.what == updateEvt)
    {
      WindowPtr target = reinterpret_cast<WindowPtr>(event.message);
      BeginUpdate(target);
      if (group_)
      {
        const std::vector<AppComponent *> &comps = group_->getComponents();
        for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
        {
          ToolboxWindow *toolboxWindow = dynamic_cast<ToolboxWindow *>(*it);
          if (toolboxWindow && toolboxWindow->window() == target)
          {
            toolboxWindow->draw();
            break;
          }
        }
      }
      EndUpdate(target);
    }
    else if (event.what == mouseDown)
    {
      WindowPtr target = 0;
      short part = FindWindow(event.where, &target);
      if (part == inGoAway && target)
      {
        if (TrackGoAway(target, event.where))
        {
          ToolboxWindow *closing = 0;
          if (group_)
          {
            const std::vector<AppComponent *> &comps = group_->getComponents();
            for (std::vector<AppComponent *>::const_iterator it = comps.begin(); it != comps.end(); ++it)
            {
              ToolboxWindow *toolboxWindow = dynamic_cast<ToolboxWindow *>(*it);
              if (toolboxWindow && toolboxWindow->window() == target)
              {
                closing = toolboxWindow;
                break;
              }
            }
          }
          if (closing)
          {
            closing->invalidateWindow();
            windowClosed(closing);
          }
        }
      }
      else if (part == inDrag && target)
      {
        Rect bounds = qd.screenBits.bounds;
        DragWindow(target, event.where, &bounds);
      }
    }
  }
}

void ToolboxApp::quit()
{
  running_ = false;
}

void ToolboxApp::windowClosed(Window *window)
{
  App::windowClosed(window);
}
