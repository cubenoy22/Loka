#include "app/core/MenuController.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/Window.hpp"
#include "app/MenuComposition.hpp"

MenuController::MenuController(AppConfigurable *config, ApplyFn applyFn, void *applyUserData)
    : config_(config),
      applyFn_(applyFn),
      applyUserData_(applyUserData),
      pendingApplyWindow_(0),
      menuBar_(0),
      refresh_(),
      diff_()
{
}

MenuController::~MenuController()
{
  menuBar_.reset();
}

void MenuController::InvalidateThunk(void *userData)
{
  MenuController *controller = static_cast<MenuController *>(userData);
  if (controller)
  {
    controller->requestInvalidation();
  }
}

bool MenuController::RefreshThunk(void *userData)
{
  MenuController *controller = static_cast<MenuController *>(userData);
  return controller ? controller->refreshDefaultMenuBar() : false;
}

void MenuController::ApplyThunk(void *userData)
{
  MenuController *controller = static_cast<MenuController *>(userData);
  if (controller)
  {
    controller->apply(controller->pendingApplyWindow_);
  }
}

void MenuController::requestInvalidation()
{
  refresh_.request();
}

bool MenuController::flushInvalidation(Window *activeWindow)
{
  pendingApplyWindow_ = activeWindow;
  return refresh_.run(&MenuController::RefreshThunk, &MenuController::ApplyThunk, this);
}

void MenuController::invalidate(Window *activeWindow)
{
  this->requestInvalidation();
  this->flushInvalidation(activeWindow);
}

void MenuController::setDefaultMenuBar(const loka::app::MenuBarDefinition *menuBar, Window *activeWindow)
{
  menuBar_.reset();
  if (menuBar)
  {
    menuBar_.reset(menuBar->clone());
  }
  this->apply(activeWindow);
}

const loka::app::MenuBarDefinition *MenuController::defaultMenuBar() const
{
  return menuBar_.get();
}

const loka::app::MenuBarDefinition *MenuController::resolveMenuBar(Window *window)
{
  if (window && window->menuBar())
  {
    return window->menuBar();
  }
  if (!diff_.valid)
  {
    this->refreshDefaultMenuBar();
  }
  return menuBar_.get();
}

bool MenuController::refreshDefaultMenuBar()
{
  if (!config_)
  {
    return false;
  }
  loka::app::MenuBarDefinition menuBar;
  loka::app::MenuComposition menuComposition(&menuBar);
  menuComposition.setInvalidateCallback(&MenuController::InvalidateThunk, this);
  config_->composeMenu(menuComposition);
  menuComposition.finish();
  std::vector<size_t> dirtyMenus;
  menuComposition.takeDirtyMenuIndices(dirtyMenus);
  if (menuBar.empty())
  {
    diff_.clear();
    if (menuBar_.isSet())
    {
      menuBar_.reset();
      diff_.valid = true;
      diff_.fullRebuild = true;
      return true;
    }
    return false;
  }
  bool diffAttempted = false;
  bool diffResult = false;
  if (!menuBar_.isSet())
  {
    diff_.valid = true;
    diff_.fullRebuild = true;
  }
  else
  {
    diffAttempted = true;
    diffResult = loka::app::MenuCompositionDiff::Diff(*menuBar_, menuBar, diff_);
    if (!diffResult)
    {
      diff_.clear();
      if (dirtyMenus.empty())
      {
        return false;
      }
    }
  }
  if (!dirtyMenus.empty())
  {
    diff_.valid = true;
    if (diff_.fullRebuild && diffAttempted && !diffResult)
    {
      diff_.fullRebuild = false;
    }
    if (!diff_.fullRebuild)
    {
      for (size_t i = 0; i < dirtyMenus.size(); ++i)
      {
        bool exists = false;
        loka::dsl::CompositionCursor<loka::app::MenuCompositionDiff::ChangedIndex> it(diff_.changedHead(),
                                                                                      diff_.changedCount());
        for (loka::app::MenuCompositionDiff::ChangedIndex *entry = it.next(); entry; entry = it.next())
        {
          if (entry->value == dirtyMenus[i])
          {
            exists = true;
            break;
          }
        }
        if (!exists)
        {
          diff_.addChanged(dirtyMenus[i]);
        }
      }
    }
  }
  if (!menuBar_.isSet() || diff_.valid)
  {
    menuBar_.reset();
    menuBar_.reset(menuBar.clone());
    return true;
  }
  diff_.clear();
  return false;
}

const loka::app::MenuCompositionDiff &MenuController::diff() const
{
  return diff_;
}

void MenuController::clearDiff()
{
  diff_.clear();
}

void MenuController::apply(Window *activeWindow)
{
  if (applyFn_)
  {
    applyFn_(applyUserData_, activeWindow);
  }
}
