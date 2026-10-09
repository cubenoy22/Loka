#ifndef LOKA_APP_MENU_COMPOSITION_HPP
#define LOKA_APP_MENU_COMPOSITION_HPP

#include <cstddef>
#include "dsl/composition/CompositionList.hpp"
#include "dsl/composition/CompositionDiff.hpp"

namespace loka
{
  namespace app
  {
    struct MenuDefinition;
    struct MenuBarDefinition;
    class MenuComposition;

    /** Clone base and replace equal-title menus in place with overlay menus;
        append overlay-only menus in order. Null means both absent or clone
        refusal; callers distinguish by input presence and keep installed truth.
        The caller owns the result; borrowed item endpoints retain their owner. */
    MenuBarDefinition *MergeMenuBars(const MenuBarDefinition *base, const MenuBarDefinition *overlay);

    struct MenuCompositionDiff : public loka::dsl::CompositionDiff
    {
      struct ChangedIndex
      {
        ChangedIndex()
            : value(0),
              nextInComposition(0)
        {
        }
        explicit ChangedIndex(size_t index)
            : value(index),
              nextInComposition(0)
        {
        }
        ChangedIndex(const ChangedIndex &other)
            : value(other.value),
              nextInComposition(0)
        {
        }
        ChangedIndex &operator=(const ChangedIndex &other)
        {
          if (this == &other)
            return *this;
          value = other.value;
          nextInComposition = 0;
          return *this;
        }
        ChangedIndex *clone() const
        {
          return new ChangedIndex(*this);
        }
        size_t value;
        ChangedIndex *nextInComposition;
      };

      MenuCompositionDiff()
          : loka::dsl::CompositionDiff(),
            changed()
      {
      }
      void clear()
      {
        loka::dsl::CompositionDiff::clear();
        changed.clear();
      }

      /** Value copies own their changed-index chain, including non-elided C++98 returns. */
      MenuCompositionDiff(const MenuCompositionDiff &other)
          : loka::dsl::CompositionDiff(other), changed()
      {
        for (ChangedIndex *entry = other.changedHead(); entry; entry = entry->nextInComposition)
          this->addChanged(entry->value);
      }
      MenuCompositionDiff &operator=(const MenuCompositionDiff &other)
      {
        if (this != &other)
        {
          MenuCompositionDiff copy(other);
          copy.changed.detachTo(this->changed);
          this->valid = copy.valid;
          this->fullRebuild = copy.fullRebuild;
        }
        return *this;
      }

      /** Diff the applied menu bar against the offered menu bar. */
      static MenuCompositionDiff DiffProjection(const MenuBarDefinition *applied, const MenuBarDefinition &offered);

      void addChanged(size_t index)
      {
        changed.appendOwned(new ChangedIndex(index));
      }
      bool hasChanged() const
      {
        return changed.count() > 0;
      }
      size_t changedCount() const
      {
        return changed.count();
      }
      ChangedIndex *changedHead() const
      {
        return changed.head();
      }

      loka::dsl::CompositionList<ChangedIndex> changed;
    };

    class MenuComposition
    {
    public:
      explicit MenuComposition(MenuBarDefinition *bar)
          : bar_(bar),
            list_()
      {
      }
      ~MenuComposition();

      void declare(const MenuDefinition &menu);
      void declare(const MenuBarDefinition &bar);
      void finish();

      MenuComposition &operator<<(const MenuDefinition &menu);
      MenuComposition &operator<<(const MenuBarDefinition &bar);

    private:
      MenuBarDefinition *bar_;
      loka::dsl::CompositionList<MenuDefinition> list_;
    };

  } // namespace app
} // namespace loka

#endif // LOKA_APP_MENU_COMPOSITION_HPP
