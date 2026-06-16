#ifndef LOKA_APP_MENU_COMPOSITION_HPP
#define LOKA_APP_MENU_COMPOSITION_HPP

#include <cassert>
#include <vector>
#include "core/State.hpp"
#include "core/util/Memory.hpp"
#include "dsl/composition/CompositionList.hpp"
#include "dsl/composition/CompositionDiff.hpp"

namespace loka
{
  namespace app
  {
    struct MenuDefinition;
    struct MenuBarDefinition;
    class MenuComposition;

    class MenuBoundary
    {
    public:
      MenuBoundary()
          : tracker_(),
            ownedStates_(),
            callbacks_()
      {
      }
      virtual ~MenuBoundary()
      {
        this->releaseCallbacks();
        for (size_t i = 0; i < ownedStates_.size(); ++i)
        {
          delete ownedStates_[i];
        }
        ownedStates_.clear();
      }
      virtual void composeMenu(MenuComposition &c) = 0;
      loka::core::StateTracker *tracker()
      {
        return &tracker_;
      }

      template <typename T> loka::core::MutableState<T> &dangerouslyUseState(const T &initial)
      {
        loka::core::MutableState<T> *state = new loka::core::MutableState<T>(initial);
        ownedStates_.push_back(state);
        tracker_.addStateUnchecked(state);
        return *state;
      }

      void reserveStates(size_t count)
      {
        ownedStates_.reserve(ownedStates_.size() + count);
        tracker_.reserveStates(count);
      }

      template <class NodeT>
      void bindActionForMenu(loka::core::EmitterState &emitter, NodeT *node, void (NodeT::*method)())
      {
        for (size_t i = 0; i < callbacks_.size(); ++i)
        {
          if (callbacks_[i] && callbacks_[i]->matches(&emitter, node, &method, sizeof(method)))
          {
            return;
          }
        }
        CallbackEntry<NodeT> *entry = new CallbackEntry<NodeT>(node, &emitter, method);
        callbacks_.push_back(entry);
        emitter.deferBind(&CallbackEntry<NodeT>::Invoke, entry);
      }

      template <class NodeT> void bindActionForMenu(loka::core::EmitterState &emitter, void (NodeT::*method)())
      {
        NodeT *self = static_cast<NodeT *>(this);
        this->bindActionForMenu(emitter, self, method);
      }

    private:
      struct CallbackEntryBase
      {
        virtual ~CallbackEntryBase() {}
        virtual void unbind() = 0;
        virtual void invalidate() = 0;
        virtual bool matches(const void *source, void *node, const void *methodBytes, size_t methodSize) const = 0;
      };

      template <class NodeT> struct CallbackEntry : public CallbackEntryBase
      {
        typedef void (NodeT::*Method)();
        CallbackEntry(NodeT *node, loka::core::EmitterState *emitter, Method method)
            : node_(node),
              emitter_(emitter),
              method_(method),
              valid_(true)
        {
        }

        static void Invoke(void *userData)
        {
          CallbackEntry *self = static_cast<CallbackEntry *>(userData);
          if (!self || !self->valid_ || !self->node_)
          {
            return;
          }
          (self->node_->*(self->method_))();
        }

        void unbind()
        {
          if (emitter_)
          {
            emitter_->deferUnbind(&Invoke, this);
          }
        }

        void invalidate()
        {
          valid_ = false;
        }

        bool matches(const void *source, void *node, const void *methodBytes, size_t methodSize) const
        {
          if (emitter_ != source || node_ != node || methodSize != sizeof(method_))
          {
            return false;
          }
          return ::loka::core::util::BytesEqual(&method_, methodBytes, sizeof(method_));
        }

        NodeT *node_;
        loka::core::EmitterState *emitter_;
        Method method_;
        bool valid_;
      };

      void releaseCallbacks()
      {
        for (size_t i = 0; i < callbacks_.size(); ++i)
        {
          callbacks_[i]->unbind();
          callbacks_[i]->invalidate();
          delete callbacks_[i];
        }
        callbacks_.clear();
      }

      loka::core::PushStateTracker tracker_;
      std::vector<loka::core::StateBase *> ownedStates_;
      std::vector<CallbackEntryBase *> callbacks_;
    };

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

      static bool Diff(const MenuBarDefinition &before, const MenuBarDefinition &after, MenuCompositionDiff &out);

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

      template <typename T> loka::core::MutableState<T> &dangerouslyUseState(const T &initial)
      {
        assert(activeBoundary_ && "MenuComposition::dangerouslyUseState requires MenuBoundary");
        return activeBoundary_->dangerouslyUseState(initial);
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
} // namespace loka

#endif // LOKA_APP_MENU_COMPOSITION_HPP
