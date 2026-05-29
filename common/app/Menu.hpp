#ifndef LOKA_APP_MENU_HPP
#define LOKA_APP_MENU_HPP

#include <vector>
#include <cassert>
#include "core/String.hpp"
#include "core/State.hpp"
#include "dsl/composition/CompositionList.hpp"
#include "app/MenuComposition.hpp"

namespace loka
{
  namespace app
  {
    struct MenuItemAttr
    {
      MenuItemAttr()
          : hasDisabledValue_(false),
            disabledValue_(false),
            disabledState_(0),
            hasVisibleValue_(false),
            visibleValue_(true),
            visibleState_(0)
      {
      }

      MenuItemAttr &disabled(bool value)
      {
        this->hasDisabledValue_ = true;
        this->disabledValue_ = value;
        this->disabledState_ = 0;
        return *this;
      }

      MenuItemAttr &disabled(loka::core::State<bool> *state)
      {
        this->hasDisabledValue_ = false;
        this->disabledState_ = state;
        return *this;
      }

      MenuItemAttr &visible(bool value)
      {
        this->hasVisibleValue_ = true;
        this->visibleValue_ = value;
        this->visibleState_ = 0;
        return *this;
      }

      MenuItemAttr &visible(loka::core::State<bool> *state)
      {
        this->hasVisibleValue_ = false;
        this->visibleState_ = state;
        return *this;
      }

      bool operator==(const MenuItemAttr &other) const
      {
        return this->hasDisabledValue_ == other.hasDisabledValue_ && this->disabledValue_ == other.disabledValue_
               && this->disabledState_ == other.disabledState_ && this->hasVisibleValue_ == other.hasVisibleValue_
               && this->visibleValue_ == other.visibleValue_ && this->visibleState_ == other.visibleState_;
      }

      bool operator<(const MenuItemAttr &other) const
      {
        if (this->hasDisabledValue_ != other.hasDisabledValue_)
          return this->hasDisabledValue_ < other.hasDisabledValue_;
        if (this->disabledValue_ != other.disabledValue_)
          return this->disabledValue_ < other.disabledValue_;
        if (this->disabledState_ != other.disabledState_)
          return this->disabledState_ < other.disabledState_;
        if (this->hasVisibleValue_ != other.hasVisibleValue_)
          return this->hasVisibleValue_ < other.hasVisibleValue_;
        if (this->visibleValue_ != other.visibleValue_)
          return this->visibleValue_ < other.visibleValue_;
        return this->visibleState_ < other.visibleState_;
      }

      bool hasDisabledValue_;
      bool disabledValue_;
      loka::core::State<bool> *disabledState_;
      bool hasVisibleValue_;
      bool visibleValue_;
      loka::core::State<bool> *visibleState_;
    };

    struct MenuItemDefinitionWithAttr;

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
            enabledValue_(true),
            hasEnabledValue_(false),
            onClickState(0),
            action(MENU_ACTION_NONE),
            isSeparator(false),
            children_(),
            hasShortcut(false),
            shortcutKey(0),
            attr_(),
            hasAttr_(false),
            nextInComposition(0)
      {
      }

      explicit MenuItemDefinition(const char *text)
          : title(loka::core::String::Literal(text)),
            enabledState(0),
            enabledValue_(true),
            hasEnabledValue_(false),
            onClickState(0),
            action(MENU_ACTION_NONE),
            isSeparator(false),
            children_(),
            hasShortcut(false),
            shortcutKey(0),
            attr_(),
            hasAttr_(false),
            nextInComposition(0)
      {
      }

      explicit MenuItemDefinition(const loka::core::String &text)
          : title(text),
            enabledState(0),
            enabledValue_(true),
            hasEnabledValue_(false),
            onClickState(0),
            action(MENU_ACTION_NONE),
            isSeparator(false),
            children_(),
            hasShortcut(false),
            shortcutKey(0),
            attr_(),
            hasAttr_(false),
            nextInComposition(0)
      {
      }

      MenuItemDefinition(const MenuItemDefinition &other)
          : title(other.title),
            enabledState(other.enabledState),
            enabledValue_(other.enabledValue_),
            hasEnabledValue_(other.hasEnabledValue_),
            onClickState(other.onClickState),
            action(other.action),
            isSeparator(other.isSeparator),
            children_(),
            hasShortcut(other.hasShortcut),
            shortcutKey(other.shortcutKey),
            attr_(other.attr_),
            hasAttr_(other.hasAttr_),
            nextInComposition(0)
      {
        const MenuItemDefinition *cur = other.children_.head();
        while (cur)
        {
          children_.appendClone(*cur);
          cur = cur->nextInComposition;
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
        enabledValue_ = other.enabledValue_;
        hasEnabledValue_ = other.hasEnabledValue_;
        onClickState = other.onClickState;
        action = other.action;
        isSeparator = other.isSeparator;
        hasShortcut = other.hasShortcut;
        shortcutKey = other.shortcutKey;
        attr_ = other.attr_;
        hasAttr_ = other.hasAttr_;
        nextInComposition = 0;
        clearChildren();
        const MenuItemDefinition *cur = other.children_.head();
        while (cur)
        {
          children_.appendClone(*cur);
          cur = cur->nextInComposition;
        }
        return *this;
      }

      MenuItemDefinition *clone() const
      {
        return new MenuItemDefinition(*this);
      }

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

      MenuItemDefinition &enabled(loka::core::State<bool> *state)
      {
        enabledState = state;
        hasEnabledValue_ = false;
        return *this;
      }

      MenuItemDefinition &enabled(bool value)
      {
        enabledState = 0;
        enabledValue_ = value;
        hasEnabledValue_ = true;
        return *this;
      }

      MenuItemDefinition &onClick(loka::core::EmitterState *emitter)
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

      MenuItemDefinitionWithAttr attr(const MenuItemAttr &value) const;

      bool isEnabledInitial() const
      {
        if (this->enabledState)
        {
          return this->enabledState->get();
        }
        if (this->hasEnabledValue_)
        {
          return this->enabledValue_;
        }
        if (!this->hasAttr_)
        {
          return true;
        }
        if (this->attr_.disabledState_)
        {
          return !this->attr_.disabledState_->get();
        }
        if (this->attr_.hasDisabledValue_)
        {
          return !this->attr_.disabledValue_;
        }
        return true;
      }

      bool isVisibleInitial() const
      {
        if (!this->hasAttr_)
        {
          return true;
        }
        if (this->attr_.visibleState_)
        {
          return this->attr_.visibleState_->get();
        }
        if (this->attr_.hasVisibleValue_)
        {
          return this->attr_.visibleValue_;
        }
        return true;
      }

      loka::core::State<bool> *enabledBindingState() const
      {
        if (this->enabledState)
        {
          return this->enabledState;
        }
        if (this->hasAttr_ && this->attr_.disabledState_)
        {
          return this->attr_.disabledState_;
        }
        return 0;
      }

      bool enabledBindingInvert() const
      {
        return (!this->enabledState && this->hasAttr_ && this->attr_.disabledState_);
      }

      MenuItemDefinition &operator<<(const MenuItemDefinition &child)
      {
        children_.appendClone(child);
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
        if (enabledValue_ != other.enabledValue_)
          return false;
        if (hasEnabledValue_ != other.hasEnabledValue_)
          return false;
        if (onClickState != other.onClickState)
          return false;
        if (action != other.action)
          return false;
        if (hasShortcut != other.hasShortcut)
          return false;
        if (shortcutKey != other.shortcutKey)
          return false;
        if (hasAttr_ != other.hasAttr_)
          return false;
        if (!(attr_ == other.attr_))
          return false;
        if (children_.count() != other.children_.count())
          return false;
        const MenuItemDefinition *left = children_.head();
        const MenuItemDefinition *right = other.children_.head();
        while (left && right)
        {
          if (!left->equalsStructure(*right))
            return false;
          left = left->nextInComposition;
          right = right->nextInComposition;
        }
        return true;
      }

      bool hasChildren() const
      {
        return children_.count() > 0;
      }
      MenuItemDefinition *childrenHead() const
      {
        return children_.head();
      }
      size_t childrenCount() const
      {
        return children_.count();
      }

      void clearChildren()
      {
        children_.clear();
      }

      loka::core::String title;
      loka::core::State<bool> *enabledState;
      bool enabledValue_;
      bool hasEnabledValue_;
      loka::core::EmitterState *onClickState;
      MenuActionType action;
      bool isSeparator;
      loka::dsl::CompositionList<MenuItemDefinition> children_;
      bool hasShortcut;
      char shortcutKey;
      MenuItemAttr attr_;
      bool hasAttr_;
      MenuItemDefinition *nextInComposition;
    };

    struct MenuItemDefinitionWithAttr : public MenuItemDefinition
    {
      MenuItemDefinitionWithAttr()
          : MenuItemDefinition()
      {
      }
      MenuItemDefinitionWithAttr(const MenuItemDefinition &base)
          : MenuItemDefinition(base)
      {
      }

    private:
      MenuItemDefinitionWithAttr attr(const MenuItemAttr &value) const;
    };

    inline MenuItemDefinitionWithAttr MenuItemDefinition::attr(const MenuItemAttr &value) const
    {
      MenuItemDefinition copy(*this);
      assert(!copy.hasAttr_ && "MenuItem.attr() can only be set once per item");
      copy.attr_ = value;
      copy.hasAttr_ = true;
      return MenuItemDefinitionWithAttr(copy);
    }

    struct MenuDefinition
    {
      MenuDefinition()
          : title(),
            isAppMenu(false),
            opaqueChildrenFlag_(false),
            opaqueChildrenSet_(false),
            items_(),
            nextInComposition(0)
      {
      }

      explicit MenuDefinition(const char *text)
          : title(loka::core::String::Literal(text)),
            isAppMenu(false),
            opaqueChildrenFlag_(false),
            opaqueChildrenSet_(false),
            items_(),
            nextInComposition(0)
      {
      }

      explicit MenuDefinition(const loka::core::String &text)
          : title(text),
            isAppMenu(false),
            opaqueChildrenFlag_(false),
            opaqueChildrenSet_(false),
            items_(),
            nextInComposition(0)
      {
      }

      MenuDefinition(const MenuDefinition &other)
          : title(other.title),
            isAppMenu(other.isAppMenu),
            opaqueChildrenFlag_(other.opaqueChildrenFlag_),
            opaqueChildrenSet_(other.opaqueChildrenSet_),
            items_(),
            nextInComposition(0)
      {
        const MenuItemDefinition *cur = other.items_.head();
        while (cur)
        {
          items_.appendClone(*cur);
          cur = cur->nextInComposition;
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
        const MenuItemDefinition *cur = other.items_.head();
        while (cur)
        {
          items_.appendClone(*cur);
          cur = cur->nextInComposition;
        }
        return *this;
      }

      MenuDefinition *clone() const
      {
        return new MenuDefinition(*this);
      }

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
        items_.appendClone(item);
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
        if (items_.count() != other.items_.count())
          return false;
        const MenuItemDefinition *left = items_.head();
        const MenuItemDefinition *right = other.items_.head();
        while (left && right)
        {
          if (!left->equalsStructure(*right))
            return false;
          left = left->nextInComposition;
          right = right->nextInComposition;
        }
        return true;
      }

      bool hasItems() const
      {
        return items_.count() > 0;
      }
      MenuItemDefinition *itemsHead() const
      {
        return items_.head();
      }
      size_t itemsCount() const
      {
        return items_.count();
      }

      void clearItems()
      {
        items_.clear();
      }

      loka::core::String title;
      bool isAppMenu;
      bool opaqueChildrenFlag_;
      bool opaqueChildrenSet_;
      loka::dsl::CompositionList<MenuItemDefinition> items_;
      MenuDefinition *nextInComposition;
    };

    struct MenuBarDefinition
    {
      MenuBarDefinition()
          : menus_()
      {
      }

      MenuBarDefinition(const MenuBarDefinition &other)
          : menus_()
      {
        const MenuDefinition *cur = other.menus_.head();
        while (cur)
        {
          menus_.appendClone(*cur);
          cur = cur->nextInComposition;
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
        const MenuDefinition *cur = other.menus_.head();
        while (cur)
        {
          menus_.appendClone(*cur);
          cur = cur->nextInComposition;
        }
        return *this;
      }

      MenuBarDefinition *clone() const
      {
        return new MenuBarDefinition(*this);
      }

      bool equalsStructure(const MenuBarDefinition &other) const
      {
        if (menus_.count() != other.menus_.count())
          return false;
        const MenuDefinition *left = menus_.head();
        const MenuDefinition *right = other.menus_.head();
        while (left && right)
        {
          if (!left->equalsStructure(*right))
            return false;
          left = left->nextInComposition;
          right = right->nextInComposition;
        }
        return true;
      }

      MenuBarDefinition &operator<<(const MenuDefinition &menu)
      {
        menus_.appendClone(menu);
        return *this;
      }

      bool empty() const
      {
        return menus_.count() == 0;
      }
      MenuDefinition *menusHead() const
      {
        return menus_.head();
      }
      size_t menusCount() const
      {
        return menus_.count();
      }
      MenuDefinition *menuAt(size_t index) const
      {
        size_t i = 0;
        MenuDefinition *cur = menus_.head();
        while (cur)
        {
          if (i == index)
          {
            return cur;
          }
          cur = cur->nextInComposition;
          i += 1;
        }
        return 0;
      }

      void clearMenus()
      {
        menus_.clear();
      }

      loka::dsl::CompositionList<MenuDefinition> menus_;
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
} // namespace loka

#endif // LOKA_APP_MENU_HPP
