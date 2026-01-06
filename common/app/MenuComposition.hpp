#ifndef LOKA_APP_MENU_COMPOSITION_HPP
#define LOKA_APP_MENU_COMPOSITION_HPP

#include <vector>
#include <cassert>
#include "core/State.hpp"
#include "loka/core/String.hpp"

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

    struct MenuCompositionDiff
    {
      MenuCompositionDiff() : valid(false), fullRebuild(true), changed() {}
      void clear()
      {
        valid = false;
        fullRebuild = true;
        changed.clear();
      }

      static bool Diff(const MenuBarDefinition &before, const MenuBarDefinition &after, MenuCompositionDiff &out);

      bool valid;
      bool fullRebuild;
      std::vector<size_t> changed;
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
            invalidateUserData_(0)
      {
      }

      void declare(const MenuDefinition &menu);
      void declare(MenuBoundary &boundary);

      void setInvalidateCallback(InvalidateFn fn, void *userData)
      {
        invalidateFn_ = fn;
        invalidateUserData_ = userData;
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
    };

  } // namespace app
} // namespace declara

#endif // LOKA_APP_MENU_COMPOSITION_HPP
