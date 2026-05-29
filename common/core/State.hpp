#ifndef LOKA_STATE_HPP
#define LOKA_STATE_HPP

#include <vector>
#include <functional>
#include <cstdio>
#include "core/StateTracker.hpp"
#include "core/String.hpp"

namespace loka
{
  namespace core
  {

    // Priority enum used for State's bind/unbind
    enum StatePriority
    {
      STATE_PRIORITY_DEFER = -1,
      STATE_PRIORITY_NORMAL = 0,
      STATE_PRIORITY_HIGH = 100
    };

    class StateTracker;
    class PushStateTracker;

    template <typename T> inline bool stateValuesEqual(const T &a, const T &b)
    {
      return !(a != b);
    }

    inline bool stateValuesEqual(const String &a, const String &b)
    {
      return a.equals(b);
    }

    // StateBase: Unified dependency management and bind API
    class StateBase
    {
    public:
      StateBase()
          : currentTracker(0),
            arenaAllocated_(false),
            lifetimeToken_(new LifetimeToken()),
            handlersVersion_(0),
            deferredHandlersVersion_(0)
      {
      }
      StateBase(const StateBase &rhs)
          : currentTracker(0),
            arenaAllocated_(rhs.arenaAllocated_),
            lifetimeToken_(new LifetimeToken()),
            handlersVersion_(0),
            deferredHandlersVersion_(0)
      {
      }
      StateBase &operator=(const StateBase &rhs)
      {
        if (this != &rhs)
        {
          // State identity, bindings, and lifetime tokens are not transferable.
          // Assignment only preserves arena placement metadata.
          currentTracker = 0;
          arenaAllocated_ = rhs.arenaAllocated_;
        }
        return *this;
      }
      virtual ~StateBase()
      {
        if (lifetimeToken_)
        {
          lifetimeToken_->alive = false;
          releaseLifetimeToken(lifetimeToken_);
          lifetimeToken_ = 0;
        }
      }
      // Enumerate dependent States (room for circular dependency detection)
      virtual std::vector<StateBase *> getDependencyStates() const
      {
        return std::vector<StateBase *>();
      }
      // Bind/subscribe API
      typedef void (*OnChangeFn)(void *userData);
      virtual void
      bind(OnChangeFn cb, void *userData, bool callImmediately = true, bool callOnce = false, int priority = 0)
      {
      }
      virtual void unbind(OnChangeFn cb, void *userData) {}
      virtual void deferBind(OnChangeFn cb, void *userData, int priority = 0) const {}
      virtual void deferUnbind(OnChangeFn cb, void *userData) const {}
      // Recompute (overridden by DerivedState)
      virtual bool recompute()
      {
        return false;
      }
      // Type query (avoid RTTI)
      virtual void *asMutableState()
      {
        return 0;
      }

    protected:
      struct LifetimeToken
      {
        int refs;
        bool alive;
        LifetimeToken()
            : refs(1),
              alive(true)
        {
        }
      };

      static void retainLifetimeToken(LifetimeToken *token)
      {
        if (token)
        {
          ++token->refs;
        }
      }

      static void releaseLifetimeToken(LifetimeToken *token)
      {
        if (!token)
        {
          return;
        }
        --token->refs;
        if (token->refs == 0)
        {
          delete token;
        }
      }

      LifetimeToken *retainNotifyToken() const
      {
        retainLifetimeToken(lifetimeToken_);
        return lifetimeToken_;
      }

      static void releaseNotifyToken(LifetimeToken *token)
      {
        releaseLifetimeToken(token);
      }

      static bool isNotifyTokenAlive(const LifetimeToken *token)
      {
        return token && token->alive;
      }

      // Handler and handler lists shared by all State specializations.
      // deferredHandlers is mutable to allow registration from const deferBind.
      struct Handler
      {
        OnChangeFn cb;
        void *userData;
        bool callOnce;
        int priority;
        bool operator==(const Handler &other) const
        {
          return cb == other.cb && userData == other.userData;
        }
      };
      std::vector<Handler> handlers;
      mutable std::vector<Handler> deferredHandlers;
      unsigned long handlersVersion_;
      mutable unsigned long deferredHandlersVersion_;

      bool containsHandler(const std::vector<Handler> &list, const Handler &target) const
      {
        for (size_t i = 0; i < list.size(); ++i)
        {
          if (list[i] == target)
          {
            return true;
          }
        }
        return false;
      }

      // Shared notification loop used by all State specializations.
      // - Snapshots both lists before iterating so bind/unbind or self-deletion
      //   inside a callback does not corrupt the loop.
      // - Skips any handler that was dynamically unbound by a sibling callback.
      void notifyHandlers()
      {
        LifetimeToken *token = retainNotifyToken();
        std::vector<Handler> snapshot = handlers;
        const unsigned long snapshotHandlersVersion = handlersVersion_;
        for (size_t i = 0; i < snapshot.size(); ++i)
        {
          const Handler &h = snapshot[i];
          if (snapshotHandlersVersion != handlersVersion_ && !containsHandler(handlers, h))
            continue;
          if (h.callOnce)
            unbind(h.cb, h.userData);
          if (h.cb)
            h.cb(h.userData);
          if (!isNotifyTokenAlive(token))
          {
            releaseNotifyToken(token);
            return;
          }
        }
        std::vector<Handler> snapshotDeferred = deferredHandlers;
        const unsigned long snapshotDeferredVersion = deferredHandlersVersion_;
        for (size_t i = 0; i < snapshotDeferred.size(); ++i)
        {
          const Handler &h = snapshotDeferred[i];
          if (snapshotDeferredVersion != deferredHandlersVersion_ && !containsHandler(deferredHandlers, h))
            continue;
          if (h.cb)
            h.cb(h.userData);
          if (!isNotifyTokenAlive(token))
          {
            releaseNotifyToken(token);
            return;
          }
        }
        releaseNotifyToken(token);
      }

      friend class PushStateTracker;
      StateTracker *currentTracker;
      bool arenaAllocated_;
      mutable LifetimeToken *lifetimeToken_;

    public:
      void setArenaAllocated(bool v)
      {
        arenaAllocated_ = v;
      }
      bool isArenaAllocated() const
      {
        return arenaAllocated_;
      }
      StateTracker *trackerOwner() const
      {
        return currentTracker;
      }
      void *retainExternalLifetimeToken() const
      {
        return retainNotifyToken();
      }
      static void releaseExternalLifetimeToken(void *token)
      {
        releaseNotifyToken(static_cast<LifetimeToken *>(token));
      }
      static bool isExternalLifetimeTokenAlive(const void *token)
      {
        return isNotifyTokenAlive(static_cast<const LifetimeToken *>(token));
      }
    };

    // State<T>: Inherits StateBase, implements value holding and subscribe API
    // bindTracker removed

    template <typename T> class State : public StateBase
    {
    public:
      State(const T &initial = T())
          : value(initial)
      {
      }
      virtual ~State() {}
      virtual T get() const
      {
        return value;
      }
      const T &getRef() const
      {
        return value;
      }

    public:
      virtual void
      bind(OnChangeFn cb, void *userData, bool callImmediately = true, bool callOnce = false, int priority = 0)
      {
        Handler h = {cb, userData, callOnce, priority};
        typename std::vector<Handler>::iterator it = this->handlers.begin();
        for (; it != this->handlers.end(); ++it)
          if (priority > it->priority)
            break;
        this->handlers.insert(it, h);
        ++this->handlersVersion_;
        if (callImmediately && cb)
        {
          cb(userData);
          if (callOnce)
            unbind(cb, userData);
        }
      }
      virtual void unbind(OnChangeFn cb, void *userData)
      {
        Handler target = {cb, userData, false, 0};
        bool removed = false;
        for (size_t i = 0; i < this->handlers.size();)
        {
          if (this->handlers[i] == target)
          {
            this->handlers.erase(this->handlers.begin() + i);
            removed = true;
            continue;
          }
          ++i;
        }
        if (removed)
        {
          ++this->handlersVersion_;
        }
      }
      virtual void deferBind(OnChangeFn cb, void *userData, int priority = 0) const
      {
        Handler h = {cb, userData, false, priority};
        typename std::vector<Handler>::iterator it = this->deferredHandlers.begin();
        for (; it != this->deferredHandlers.end(); ++it)
          if (priority > it->priority)
            break;
        this->deferredHandlers.insert(it, h);
        ++this->deferredHandlersVersion_;
      }
      virtual void deferUnbind(OnChangeFn cb, void *userData) const
      {
        Handler target = {cb, userData, false, 0};
        bool removed = false;
        for (size_t i = 0; i < this->deferredHandlers.size();)
        {
          if (this->deferredHandlers[i] == target)
          {
            this->deferredHandlers.erase(this->deferredHandlers.begin() + i);
            removed = true;
            continue;
          }
          ++i;
        }
        if (removed)
        {
          ++this->deferredHandlersVersion_;
        }
      }

    protected:
      typedef StateBase::Handler Handler;

      virtual void set(const T &v)
      {
        if (!stateValuesEqual(value, v))
        {
          value = v;
          notifyStateChanged();
        }
      }
      virtual void setValue(const T &v)
      {
        set(v);
      }
      // setValue(const ValueHolderBase&) removed as no longer needed
      void notifyStateChanged()
      {
        notifyHandlers();
      }

      T value;
    };

    // State<void> specialization: No value held, event propagation only
    // - Generic State for expressing UI events (button clicks, notifications, triggers, etc.) or "state changes without
    // value"
    // - Also used as base for event-based States like EmitterState
    // - Realizes "type-safe event propagation" everywhere: tests, app body, extension libraries, etc.
    // Has the same bind API and handler management as State<T>
    //
    template <> class State<void> : public StateBase
    {
    public:
      State() {}
      virtual ~State() {}

      virtual void
      bind(OnChangeFn cb, void *userData, bool callImmediately = true, bool callOnce = false, int priority = 0)
      {
        Handler h = {cb, userData, callOnce, priority};
        std::vector<Handler>::iterator it = handlers.begin();
        for (; it != handlers.end(); ++it)
          if (priority > it->priority)
            break;
        handlers.insert(it, h);
        ++handlersVersion_;
        if (callImmediately && cb)
        {
          cb(userData);
          if (callOnce)
            unbind(cb, userData);
        }
      }
      virtual void unbind(OnChangeFn cb, void *userData)
      {
        Handler target = {cb, userData, false, 0};
        bool removed = false;
        for (size_t i = 0; i < handlers.size();)
        {
          if (handlers[i] == target)
          {
            handlers.erase(handlers.begin() + i);
            removed = true;
            continue;
          }
          ++i;
        }
        if (removed)
        {
          ++handlersVersion_;
        }
      }
      virtual void deferBind(OnChangeFn cb, void *userData, int priority = 0) const
      {
        Handler h = {cb, userData, false, priority};
        std::vector<Handler>::iterator it = deferredHandlers.begin();
        for (; it != deferredHandlers.end(); ++it)
          if (priority > it->priority)
            break;
        deferredHandlers.insert(it, h);
        ++deferredHandlersVersion_;
      }
      virtual void deferUnbind(OnChangeFn cb, void *userData) const
      {
        Handler target = {cb, userData, false, 0};
        bool removed = false;
        for (size_t i = 0; i < deferredHandlers.size();)
        {
          if (deferredHandlers[i] == target)
          {
            deferredHandlers.erase(deferredHandlers.begin() + i);
            removed = true;
            continue;
          }
          ++i;
        }
        if (removed)
        {
          ++deferredHandlersVersion_;
        }
      }

    protected:
      // Event notification API - Used by derived classes like EmitterState
      void notifyStateChanged()
      {
        notifyHandlers();
      }
    };

    // --- EmitterState: Pure event State without value ---
    //
    // Design intent:
    // - EmitterState is a pure event State that only receives OS/platform events (e.g., button clicks) via emit().
    // - clickEvent held by SceneNodeButton etc. is called by OS callbacks from each platform's SceneNodeContext.
    // - On the Loka side, events are propagated to all parties that bindDefer to this EmitterState.
    // - EmitterState holds no value or flags, just calls notifyStateChanged() via emit().
    // - Users don't need to worry about emitted() or consume(), just describe side effects with bindDefer.
    class EmitterState : public State<void>
    {
    public:
      EmitterState() {}
      void emit()
      {
        notifyStateChanged();
      }
    };

    // --- New: MutableState ---
    template <typename T> class MutableState : public State<T>
    {
    public:
      MutableState()
          : State<T>()
      {
      }
      explicit MutableState(const T &initial)
          : State<T>(initial)
      {
      }
      virtual void *asMutableState()
      {
        return this;
      }
      using State<T>::setValue;
      void set(const T &v, bool forceUpdate = false)
      {
        StateBase::LifetimeToken *token = this->retainNotifyToken();
        if (forceUpdate)
        {
          State<T>::set(v);
          if (!StateBase::isNotifyTokenAlive(token))
          {
            StateBase::releaseNotifyToken(token);
            return;
          }
          this->notifyStateChanged(); // Always notify even when the value is unchanged.
          if (!StateBase::isNotifyTokenAlive(token))
          {
            StateBase::releaseNotifyToken(token);
            return;
          }
        }
        else
        {
          State<T>::set(v);
          if (!StateBase::isNotifyTokenAlive(token))
          {
            StateBase::releaseNotifyToken(token);
            return;
          }
        }
        if (this->currentTracker)
          this->currentTracker->markDirty(this);
        StateBase::releaseNotifyToken(token);
      }
    };

    // --- DerivedState: C++98-compatible implementation of State<T> ---
    template <typename T> class DerivedState : public State<T>
    {
    public:
      // Pure virtual base class for evaluation expression (C++98-compatible: operator() can be const reference)
      struct EvalFn
      {
        virtual ~EvalFn() {}
        virtual T operator()() = 0;
      };
      DerivedState(const std::vector<StateBase *> &deps, EvalFn *eval)
          : State<T>(eval ? (*eval)() : T()),
            dependencies(deps),
            evalFn(eval)
      {
        this->value = evalFn ? (*evalFn)() : T();
      }
      DerivedState(StateBase *dep, EvalFn *eval)
          : State<T>(eval ? (*eval)() : T()),
            evalFn(eval)
      {
        if (dep)
          dependencies.push_back(dep);
        this->value = evalFn ? (*evalFn)() : T();
      }
      DerivedState(StateBase *dep1, StateBase *dep2, EvalFn *eval)
          : State<T>(eval ? (*eval)() : T()),
            evalFn(eval)
      {
        dependencies.reserve(2);
        if (dep1)
          dependencies.push_back(dep1);
        if (dep2 && dep2 != dep1)
          dependencies.push_back(dep2);
        this->value = evalFn ? (*evalFn)() : T();
      }
      virtual ~DerivedState()
      {
        delete evalFn;
        evalFn = 0;
      }

      virtual std::vector<StateBase *> getDependencyStates() const
      {
        return dependencies;
      }

    private:
      void set(const T &v) {}
      void setValue(const T &v) {}
      bool recompute()
      {
        if (!evalFn)
          return false;
        T newVal = (*evalFn)();
        if (!stateValuesEqual(newVal, this->value))
        {
          this->value = newVal;
          this->notifyStateChanged();
          return true;
        }
        return false;
      }
      std::vector<StateBase *> dependencies;
      EvalFn *evalFn;
    };

  } // namespace core
} // namespace loka

#endif // LOKA_STATE_HPP
