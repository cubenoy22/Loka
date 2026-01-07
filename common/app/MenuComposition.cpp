#include "app/MenuComposition.hpp"
#include "app/Menu.hpp"

namespace declara
{
  namespace app
  {
    bool MenuCompositionDiff::Diff(const MenuBarDefinition &before, const MenuBarDefinition &after, MenuCompositionDiff &out)
    {
      out.clear();
      out.fullRebuild = false;
      if (before.menus.size() != after.menus.size())
      {
        out.valid = true;
        out.fullRebuild = true;
        return true;
      }
      for (size_t i = 0; i < before.menus.size(); ++i)
      {
        if (!before.menus[i] || !after.menus[i])
        {
          out.valid = true;
          out.fullRebuild = true;
          return true;
        }
        if (!before.menus[i]->equalsStructure(*after.menus[i]))
        {
          out.changed.push_back(i);
        }
      }
      if (!out.changed.empty())
      {
        out.valid = true;
        return true;
      }
      return false;
    }

    MenuComposition::~MenuComposition()
    {
      // list_ cleans up automatically
    }

    void MenuComposition::declare(const MenuDefinition &menu)
    {
      if (bar_)
      {
        MenuDefinition copy(menu);
        if (boundaryDepth_ > 0 && !copy.opaqueChildrenSet_)
          copy.opaqueChildren(true);
        list_.appendOwned(new MenuDefinition(copy));
      }
    }

    void MenuComposition::declare(const MenuBarDefinition &bar)
    {
      if (!bar_)
        return;
      for (size_t i = 0; i < bar.menus.size(); ++i)
      {
        if (!bar.menus[i])
          continue;
        declare(*bar.menus[i]);
      }
    }

    void MenuComposition::declare(MenuBoundary &boundary)
    {
      if (!bar_)
        return;
      boundaryDepth_ += 1;
      MenuBoundary *prevBoundary = activeBoundary_;
      activeBoundary_ = &boundary;
      declara::core::PushStateTracker *tracker = static_cast<declara::core::PushStateTracker *>(boundary.tracker());
      if (tracker)
      {
        tracker->begin();
      }
      boundary.composeMenu(*this);
      if (tracker)
      {
        tracker->end();
        if (tracker->consumeDirty() && invalidateFn_)
        {
          invalidateFn_(invalidateUserData_);
        }
      }
      activeBoundary_ = prevBoundary;
      boundaryDepth_ -= 1;
    }

    void MenuComposition::finish()
    {
      if (!bar_ || list_.count() == 0)
        return;
      bar_->clearMenus();
      bar_->reserve(list_.count());
      list_.detachTo(bar_->menus);
    }

    MenuComposition &MenuComposition::operator<<(const MenuDefinition &menu)
    {
      declare(menu);
      return *this;
    }

    MenuComposition &MenuComposition::operator<<(const MenuBarDefinition &bar)
    {
      declare(bar);
      return *this;
    }

    MenuComposition &MenuComposition::operator<<(MenuBoundary &boundary)
    {
      declare(boundary);
      return *this;
    }
  } // namespace app
} // namespace declara
