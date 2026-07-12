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
      diff_(),
      pendingDirtyMenus_(),
      retryCloneRequested_(false)
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
  bool changed = refresh_.run(&MenuController::RefreshThunk, &MenuController::ApplyThunk, this);
  if (retryCloneRequested_)
  {
    retryCloneRequested_ = false;
    refresh_.request();
  }
  return changed;
}

void MenuController::invalidate(Window *activeWindow)
{
  this->requestInvalidation();
  this->flushInvalidation(activeWindow);
}

void MenuController::setDefaultMenuBar(const loka::app::MenuBarDefinition *menuBar, Window *activeWindow)
{
  if (menuBar)
  {
    loka::core::OwnedDef<loka::app::MenuBarDefinition> nextMenuBar(menuBar->clone());
    if (!nextMenuBar.isSet())
    {
      return;
    }
    menuBar_.reset(nextMenuBar.take());
  }
  else
  {
    menuBar_.reset();
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

// Refresh is one transaction with a single commit point at the end:
//   1. Recompose the candidate bar. This CONSUMES the boundary dirty flags
//      (MenuComposition::declare -> consumeDirty), so from here on the dirty
//      set exists only in `dirtyMenus`, merged with any entries requeued by a
//      previously failed commit.
//   2. Build the candidate diff into the local `nextDiff`; `menuBar_` and
//      `diff_` stay untouched while anything can still fail.
//   3. Clone the bar and commit bar + diff together. On clone failure nothing
//      is published: the consumed dirty set is stashed into
//      `pendingDirtyMenus_` and a retry is scheduled after this run (see
//      flushInvalidation), because NextTickTracker::run drains re-requests in
//      a loop within the current flush.
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
  if (!pendingDirtyMenus_.empty())
  {
    for (size_t i = 0; i < pendingDirtyMenus_.size(); ++i)
    {
      bool exists = false;
      for (size_t j = 0; j < dirtyMenus.size(); ++j)
      {
        if (dirtyMenus[j] == pendingDirtyMenus_[i])
        {
          exists = true;
          break;
        }
      }
      if (!exists)
      {
        dirtyMenus.push_back(pendingDirtyMenus_[i]);
      }
    }
    pendingDirtyMenus_.clear();
  }
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
  loka::app::MenuCompositionDiff nextDiff;
  bool diffAttempted = false;
  bool diffResult = false;
  if (!menuBar_.isSet())
  {
    nextDiff.valid = true;
    nextDiff.fullRebuild = true;
  }
  else
  {
    diffAttempted = true;
    diffResult = loka::app::MenuCompositionDiff::Diff(*menuBar_, menuBar, nextDiff);
    if (!diffResult)
    {
      if (dirtyMenus.empty())
      {
        diff_.clear();
        return false;
      }
    }
  }
  if (!dirtyMenus.empty())
  {
    nextDiff.valid = true;
    if (nextDiff.fullRebuild && diffAttempted && !diffResult)
    {
      nextDiff.fullRebuild = false;
    }
    if (!nextDiff.fullRebuild)
    {
      for (size_t i = 0; i < dirtyMenus.size(); ++i)
      {
        bool exists = false;
        loka::dsl::CompositionCursor<loka::app::MenuCompositionDiff::ChangedIndex> it(nextDiff.changedHead(),
                                                                                      nextDiff.changedCount());
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
          nextDiff.addChanged(dirtyMenus[i]);
        }
      }
    }
  }
  if (!menuBar_.isSet() || nextDiff.valid)
  {
    loka::core::OwnedDef<loka::app::MenuBarDefinition> nextMenuBar(menuBar.clone());
    if (!nextMenuBar.isSet())
    {
      // The compose above consumed the boundary dirty flags; without
      // requeueing them the retry would see a clean, structurally equal bar
      // and drop this update.
      pendingDirtyMenus_.swap(dirtyMenus);
      if (refresh_.inProgress())
      {
        // Inside NextTickTracker::run a request() would re-enter the drain
        // loop immediately; defer to flushInvalidation so a persistent
        // failure costs one attempt per flush.
        retryCloneRequested_ = true;
      }
      else
      {
        // Direct call (e.g. via resolveMenuBar): nobody will check the
        // retry flag, so schedule the retry now.
        refresh_.request();
      }
      return false;
    }
    menuBar_.reset(nextMenuBar.take());
    diff_.clear();
    diff_.valid = nextDiff.valid;
    diff_.fullRebuild = nextDiff.fullRebuild;
    nextDiff.changed.detachTo(diff_.changed);
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
