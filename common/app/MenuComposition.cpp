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
      if (listHead_)
      {
        MenuDefinition *node = listHead_;
        while (node)
        {
          MenuDefinition *next = node->nextInComposition;
          delete node;
          node = next;
        }
      }
    }

    void MenuComposition::declare(const MenuDefinition &menu)
    {
      if (bar_)
      {
        MenuDefinition copy(menu);
        if (boundaryDepth_ > 0 && !copy.opaqueChildrenSet_)
          copy.opaqueChildren(true);
        MenuDefinition *node = new MenuDefinition(copy);
        node->nextInComposition = 0;
        if (!listHead_)
        {
          listHead_ = node;
          listTail_ = node;
        }
        else
        {
          listTail_->nextInComposition = node;
          listTail_ = node;
        }
        listCount_ += 1;
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
      if (!bar_ || !listHead_)
        return;
      bar_->clearMenus();
      bar_->reserve(listCount_);
      MenuDefinition *node = listHead_;
      while (node)
      {
        MenuDefinition *next = node->nextInComposition;
        node->nextInComposition = 0;
        bar_->menus.push_back(node);
        node = next;
      }
      listHead_ = 0;
      listTail_ = 0;
      listCount_ = 0;
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
