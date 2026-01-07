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
      if (before.menusCount() != after.menusCount())
      {
        out.valid = true;
        out.fullRebuild = true;
        return true;
      }
      loka::dsl::CompositionCursor<MenuDefinition> beforeIt(before.menusHead(), before.menusCount());
      loka::dsl::CompositionCursor<MenuDefinition> afterIt(after.menusHead(), after.menusCount());
      size_t index = 0;
      for (MenuDefinition *beforeMenu = beforeIt.next(), *afterMenu = afterIt.next();
           beforeMenu && afterMenu;
           beforeMenu = beforeIt.next(), afterMenu = afterIt.next(), ++index)
      {
        if (!beforeMenu->equalsStructure(*afterMenu))
        {
          out.addChanged(index);
        }
      }
      if (out.hasChanged())
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
      loka::dsl::CompositionCursor<MenuDefinition> it(bar.menusHead(), bar.menusCount());
      for (MenuDefinition *menu = it.next(); menu; menu = it.next())
      {
        declare(*menu);
      }
    }

    void MenuComposition::declare(MenuBoundary &boundary)
    {
      if (!bar_)
        return;
      boundaryDepth_ += 1;
      size_t countBefore = list_.count();
      MenuBoundary *prevBoundary = activeBoundary_;
      activeBoundary_ = &boundary;
      declara::core::PushStateTracker *tracker = static_cast<declara::core::PushStateTracker *>(boundary.tracker());
      if (tracker)
      {
        tracker->begin();
      }
      boundary.composeMenu(*this);
      bool boundaryDirty = false;
      if (tracker)
      {
        tracker->end();
        boundaryDirty = tracker->consumeDirty();
        if (boundaryDirty && invalidateFn_)
        {
          invalidateFn_(invalidateUserData_);
        }
      }
      if (boundaryDirty)
      {
        size_t countAfter = list_.count();
        for (size_t i = countBefore; i < countAfter; ++i)
        {
          bool exists = false;
          for (size_t j = 0; j < dirtyIndices_.size(); ++j)
          {
            if (dirtyIndices_[j] == i)
            {
              exists = true;
              break;
            }
          }
          if (!exists)
          {
            dirtyIndices_.push_back(i);
          }
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
      list_.detachTo(bar_->menus_);
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
