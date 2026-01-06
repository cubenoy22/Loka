#ifndef LOKA_APP_MENU_HPP
#define LOKA_APP_MENU_HPP

#include <vector>
#include "loka/core/String.hpp"
#include "app/MenuComposition.hpp"

namespace declara
{
  namespace app
  {
    enum MenuActionType
    {
      MENU_ACTION_NONE = 0,
      MENU_ACTION_ABOUT_APP,
      MENU_ACTION_QUIT_APP,
      MENU_ACTION_SHOW_COLOR_PICKER,
      MENU_ACTION_REBUILD_MENU
    };

    struct MenuItemDefinition
    {
      MenuItemDefinition()
          : title(),
            enabledState(0),
            onClickState(0),
            action(MENU_ACTION_NONE),
            isSeparator(false),
            children(),
            hasShortcut(false),
            shortcutKey(0)
      {
      }

      explicit MenuItemDefinition(const char *text)
          : title(loka::core::String::Literal(text)),
            enabledState(0),
            onClickState(0),
            action(MENU_ACTION_NONE),
            isSeparator(false),
            children(),
            hasShortcut(false),
            shortcutKey(0)
      {
      }

      explicit MenuItemDefinition(const loka::core::String &text)
          : title(text),
            enabledState(0),
            onClickState(0),
            action(MENU_ACTION_NONE),
            isSeparator(false),
            children(),
            hasShortcut(false),
            shortcutKey(0)
      {
      }

      MenuItemDefinition(const MenuItemDefinition &other)
          : title(other.title),
            enabledState(other.enabledState),
            onClickState(other.onClickState),
            action(other.action),
            isSeparator(other.isSeparator),
            children(),
            hasShortcut(other.hasShortcut),
            shortcutKey(other.shortcutKey)
      {
        for (size_t i = 0; i < other.children.size(); ++i)
        {
          children.push_back(other.children[i]->clone());
        }
      }

      ~MenuItemDefinition()
      {
        clearChildren();
      }

      MenuItemDefinition &operator=(const MenuItemDefinition &other)
      {
        if (this == &other)
          return *this;
        title = other.title;
        enabledState = other.enabledState;
        onClickState = other.onClickState;
        action = other.action;
        isSeparator = other.isSeparator;
        hasShortcut = other.hasShortcut;
        shortcutKey = other.shortcutKey;
        clearChildren();
        for (size_t i = 0; i < other.children.size(); ++i)
        {
          children.push_back(other.children[i]->clone());
        }
        return *this;
      }

      MenuItemDefinition *clone() const { return new MenuItemDefinition(*this); }

      MenuItemDefinition &text(const loka::core::String &value)
      {
        title = value;
        return *this;
      }

      MenuItemDefinition &text(const char *value)
      {
        title = loka::core::String::Literal(value);
        return *this;
      }

      MenuItemDefinition &enabled(State<bool> *state)
      {
        enabledState = state;
        return *this;
      }

      MenuItemDefinition &enabled(bool value)
      {
        enabledState = declara::core::StaticState<bool>(value);
        return *this;
      }

      MenuItemDefinition &onClick(EmitterState *emitter)
      {
        onClickState = emitter;
        return *this;
      }

      MenuItemDefinition &actionType(MenuActionType type)
      {
        action = type;
        return *this;
      }

      MenuItemDefinition &separator()
      {
        isSeparator = true;
        return *this;
      }

      MenuItemDefinition &shortcut(char key)
      {
        hasShortcut = true;
        shortcutKey = key;
        return *this;
      }

      MenuItemDefinition &operator<<(const MenuItemDefinition &child)
      {
        children.push_back(child.clone());
        return *this;
      }

      bool equalsStructure(const MenuItemDefinition &other) const
      {
        if (isSeparator != other.isSeparator)
          return false;
        if (!title.equals(other.title))
          return false;
        if (enabledState != other.enabledState)
          return false;
        if (onClickState != other.onClickState)
          return false;
        if (action != other.action)
          return false;
        if (hasShortcut != other.hasShortcut)
          return false;
        if (shortcutKey != other.shortcutKey)
          return false;
        if (children.size() != other.children.size())
          return false;
        for (size_t i = 0; i < children.size(); ++i)
        {
          if (!children[i]->equalsStructure(*other.children[i]))
            return false;
        }
        return true;
      }

      bool hasChildren() const { return !children.empty(); }

      void clearChildren()
      {
        for (size_t i = 0; i < children.size(); ++i)
        {
          delete children[i];
        }
        children.clear();
      }

      loka::core::String title;
      State<bool> *enabledState;
      EmitterState *onClickState;
      MenuActionType action;
      bool isSeparator;
      std::vector<MenuItemDefinition *> children;
      bool hasShortcut;
      char shortcutKey;
    };

    struct MenuDefinition
    {
      MenuDefinition()
          : title(),
            isAppMenu(false),
            opaqueChildrenFlag_(false),
            opaqueChildrenSet_(false),
            items(),
            nextInComposition(0)
      {
      }

      explicit MenuDefinition(const char *text)
          : title(loka::core::String::Literal(text)),
            isAppMenu(false),
            opaqueChildrenFlag_(false),
            opaqueChildrenSet_(false),
            items(),
            nextInComposition(0)
      {
      }

      explicit MenuDefinition(const loka::core::String &text)
          : title(text),
            isAppMenu(false),
            opaqueChildrenFlag_(false),
            opaqueChildrenSet_(false),
            items(),
            nextInComposition(0)
      {
      }

      MenuDefinition(const MenuDefinition &other)
          : title(other.title),
            isAppMenu(other.isAppMenu),
            opaqueChildrenFlag_(other.opaqueChildrenFlag_),
            opaqueChildrenSet_(other.opaqueChildrenSet_),
            items(),
            nextInComposition(0)
      {
        for (size_t i = 0; i < other.items.size(); ++i)
        {
          items.push_back(other.items[i]->clone());
        }
      }

      ~MenuDefinition()
      {
        clearItems();
      }

      MenuDefinition &operator=(const MenuDefinition &other)
      {
        if (this == &other)
          return *this;
        title = other.title;
        isAppMenu = other.isAppMenu;
        opaqueChildrenFlag_ = other.opaqueChildrenFlag_;
        opaqueChildrenSet_ = other.opaqueChildrenSet_;
        nextInComposition = 0;
        clearItems();
        for (size_t i = 0; i < other.items.size(); ++i)
        {
          items.push_back(other.items[i]->clone());
        }
        return *this;
      }

      MenuDefinition *clone() const { return new MenuDefinition(*this); }

      MenuDefinition &text(const loka::core::String &value)
      {
        title = value;
        return *this;
      }

      MenuDefinition &text(const char *value)
      {
        title = loka::core::String::Literal(value);
        return *this;
      }

      MenuDefinition &appMenu(bool flag)
      {
        isAppMenu = flag;
        return *this;
      }

      MenuDefinition &opaqueChildren(bool flag)
      {
        opaqueChildrenFlag_ = flag;
        opaqueChildrenSet_ = true;
        return *this;
      }

      MenuDefinition &operator<<(const MenuItemDefinition &item)
      {
        items.push_back(item.clone());
        return *this;
      }

      bool equalsStructure(const MenuDefinition &other) const
      {
        if (isAppMenu != other.isAppMenu)
          return false;
        if (!title.equals(other.title))
          return false;
        if (opaqueChildrenFlag_ != other.opaqueChildrenFlag_)
          return false;
        if (opaqueChildrenFlag_)
          return true;
        if (items.size() != other.items.size())
          return false;
        for (size_t i = 0; i < items.size(); ++i)
        {
          if (!items[i]->equalsStructure(*other.items[i]))
            return false;
        }
        return true;
      }

      bool hasItems() const { return !items.empty(); }

      void clearItems()
      {
        for (size_t i = 0; i < items.size(); ++i)
        {
          delete items[i];
        }
        items.clear();
      }

      loka::core::String title;
      bool isAppMenu;
      bool opaqueChildrenFlag_;
      bool opaqueChildrenSet_;
      std::vector<MenuItemDefinition *> items;
      MenuDefinition *nextInComposition;
    };

    struct MenuBarDefinition
    {
      MenuBarDefinition() : menus() {}

      MenuBarDefinition(const MenuBarDefinition &other) : menus()
      {
        for (size_t i = 0; i < other.menus.size(); ++i)
        {
          menus.push_back(other.menus[i]->clone());
        }
      }

      ~MenuBarDefinition()
      {
        clearMenus();
      }

      MenuBarDefinition &operator=(const MenuBarDefinition &other)
      {
        if (this == &other)
          return *this;
        clearMenus();
        for (size_t i = 0; i < other.menus.size(); ++i)
        {
          menus.push_back(other.menus[i]->clone());
        }
        return *this;
      }

      MenuBarDefinition *clone() const { return new MenuBarDefinition(*this); }

      bool equalsStructure(const MenuBarDefinition &other) const
      {
        if (menus.size() != other.menus.size())
          return false;
        for (size_t i = 0; i < menus.size(); ++i)
        {
          if (!menus[i]->equalsStructure(*other.menus[i]))
            return false;
        }
        return true;
      }

      MenuBarDefinition &operator<<(const MenuDefinition &menu)
      {
        menus.push_back(menu.clone());
        return *this;
      }

      void reserve(size_t count)
      {
        menus.reserve(count);
      }

      bool empty() const { return menus.empty(); }

      void clearMenus()
      {
        for (size_t i = 0; i < menus.size(); ++i)
        {
          delete menus[i];
        }
        menus.clear();
      }

      std::vector<MenuDefinition *> menus;
    };

    // MenuComposition, MenuBoundary, MenuCompositionDiff are defined in MenuComposition.hpp.

    inline MenuDefinition Menu(const char *title)
    {
      return MenuDefinition(title);
    }

    inline MenuDefinition Menu(const loka::core::String &title)
    {
      return MenuDefinition(title);
    }

    inline MenuDefinition AppMenu()
    {
      MenuDefinition menu("App");
      menu.appMenu(true);
      return menu;
    }

    inline MenuItemDefinition MenuItem(const char *title)
    {
      return MenuItemDefinition(title);
    }

    inline MenuItemDefinition MenuItem(const loka::core::String &title)
    {
      return MenuItemDefinition(title);
    }

    inline MenuItemDefinition MenuSeparator()
    {
      return MenuItemDefinition().separator();
    }

  } // namespace app
} // namespace declara

#endif // LOKA_APP_MENU_HPP
