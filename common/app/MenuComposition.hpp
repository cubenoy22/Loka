#ifndef LOKA_APP_MENU_COMPOSITION_HPP
#define LOKA_APP_MENU_COMPOSITION_HPP

#include <cassert>
#include <vector>
#include "core/State.hpp"
#include "loka/dsl/CompositionList.hpp"
#include "loka/dsl/CompositionDiff.hpp"

namespace declara
{
  namespace app
  {
    struct MenuDefinition;
    struct MenuBarDefinition;
    class MenuComposition;

    class MenuBoundary
    {
    public:
      MenuBoundary() : tracker_(), ownedStates_() {}
      virtual ~MenuBoundary()
      {
        for (size_t i = 0; i < ownedStates_.size(); ++i)
        {
          delete ownedStates_[i];
        }
        ownedStates_.clear();
      }
      virtual void composeMenu(MenuComposition &c) = 0;
      declara::core::StateTracker *tracker() { return &tracker_; }

      template <typename T>
      declara::core::MutableState<T> &useState(const T &initial)
      {
        declara::core::MutableState<T> *state = new declara::core::MutableState<T>(initial);
        ownedStates_.push_back(state);
        tracker_.addState(state);
        return *state;
      }

    private:
      declara::core::PushStateTracker tracker_;
      std::vector<declara::core::StateBase *> ownedStates_;
    };

    struct MenuCompositionDiff : public loka::dsl::CompositionDiff
    {
      struct ChangedIndex
      {
        ChangedIndex() : value(0), nextInComposition(0) {}
        explicit ChangedIndex(size_t index) : value(index), nextInComposition(0) {}
        ChangedIndex(const ChangedIndex &other) : value(other.value), nextInComposition(0) {}
        ChangedIndex &operator=(const ChangedIndex &other)
        {
          if (this == &other)
            return *this;
          value = other.value;
          nextInComposition = 0;
          return *this;
        }
        ChangedIndex *clone() const { return new ChangedIndex(*this); }
        size_t value;
        ChangedIndex *nextInComposition;
      };

      MenuCompositionDiff() : loka::dsl::CompositionDiff(), changed() {}
      void clear()
      {
        loka::dsl::CompositionDiff::clear();
        changed.clear();
      }

      static bool Diff(const MenuBarDefinition &before, const MenuBarDefinition &after, MenuCompositionDiff &out);

      void addChanged(size_t index)
      {
        changed.appendOwned(new ChangedIndex(index));
      }
      bool hasChanged() const { return changed.count() > 0; }
      size_t changedCount() const { return changed.count(); }
      ChangedIndex *changedHead() const { return changed.head(); }

      loka::dsl::CompositionList<ChangedIndex> changed;
    };

    class MenuComposition
    {
    public:
      typedef void (*InvalidateFn)(void *userData);

      explicit MenuComposition(MenuBarDefinition *bar)
          : bar_(bar),
            boundaryDepth_(0),
            activeBoundary_(0),
            invalidateFn_(0),
            invalidateUserData_(0),
            list_(),
            dirtyIndices_()
      {
      }
      ~MenuComposition();

      void declare(const MenuDefinition &menu);
      void declare(const MenuBarDefinition &bar);
      void declare(MenuBoundary &boundary);
      void finish();

      MenuComposition &operator<<(const MenuDefinition &menu);
      MenuComposition &operator<<(const MenuBarDefinition &bar);
      MenuComposition &operator<<(MenuBoundary &boundary);

      void setInvalidateCallback(InvalidateFn fn, void *userData)
      {
        invalidateFn_ = fn;
        invalidateUserData_ = userData;
      }

      void takeDirtyMenuIndices(std::vector<size_t> &out)
      {
        out.clear();
        out.swap(dirtyIndices_);
      }

      template <typename T>
      declara::core::MutableState<T> &useState(const T &initial)
      {
        assert(activeBoundary_ && "MenuComposition::useState requires MenuBoundary");
        return activeBoundary_->useState(initial);
      }

    private:
      MenuBarDefinition *bar_;
      int boundaryDepth_;
      MenuBoundary *activeBoundary_;
      InvalidateFn invalidateFn_;
      void *invalidateUserData_;
      loka::dsl::CompositionList<MenuDefinition> list_;
      std::vector<size_t> dirtyIndices_;
    };

  } // namespace app
} // namespace declara

#endif // LOKA_APP_MENU_COMPOSITION_HPP
