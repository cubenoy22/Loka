#ifndef LOKA_STATE_HPP
#define LOKA_STATE_HPP

#include <vector>
#include <functional>
#include "StateTracker.hpp"
#include "loka/core/String.hpp"

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

    template <typename T>
    inline bool stateValuesEqual(const T &a, const T &b)
    {
      return !(a != b);
    }

    inline bool stateValuesEqual(const loka::core::String &a, const loka::core::String &b)
    {
      return a.equals(b);
    }

    // StateBase: Unified dependency management and bind API
    class StateBase
    {
    public:
      StateBase() : currentTracker(0), arenaAllocated_(false) {}
      virtual ~StateBase() {}
      // Enumerate dependent States (room for circular dependency detection)
      virtual std::vector<StateBase *> getDependencyStates() const { return std::vector<StateBase *>(); }
      // Bind/subscribe API
      typedef void (*OnChangeFn)(void *userData);
      virtual void bind(OnChangeFn cb, void *userData, bool callImmediately = true, bool callOnce = false, int priority = 0) {}
      virtual void unbind(OnChangeFn cb, void *userData) {}
      virtual void deferBind(OnChangeFn cb, void *userData, int priority = 0) const {}
      virtual void deferUnbind(OnChangeFn cb, void *userData) const {}
      // Recompute (overridden by DerivedState)
      virtual bool recompute() { return false; }
      // Type query (avoid RTTI)
      virtual void *asMutableState() { return 0; }

    protected:
      friend class PushStateTracker;
      StateTracker *currentTracker;
      bool arenaAllocated_;

    public:
      void setArenaAllocated(bool v) { arenaAllocated_ = v; }
      bool isArenaAllocated() const { return arenaAllocated_; }
    };

    // State<T>: Inherits StateBase, implements value holding and subscribe API
    // bindTracker removed

    template <typename T>
    class State : public StateBase
    {
    public:
      State(const T &initial = T()) : value(initial) {}
      virtual ~State() {}
      virtual T get() const { return value; }
      const T &getRef() const { return value; }

    public:
      virtual void bind(OnChangeFn cb, void *userData, bool callImmediately = true, bool callOnce = false, int priority = 0)
      {
        Handler h = {cb, userData, callOnce, priority};
        typename std::vector<Handler>::iterator it = handlers.begin();
        for (; it != handlers.end(); ++it)
          if (priority > it->priority)
            break;
        handlers.insert(it, h);
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
        for (size_t i = 0; i < handlers.size(); ++i)
        {
          if (handlers[i] == target)
          {
            handlers.erase(handlers.begin() + i);
            break;
          }
        }
      }
      virtual void deferBind(OnChangeFn cb, void *userData, int priority = 0) const
      {
        Handler h = {cb, userData, false, priority};
        std::vector<Handler> &ref = const_cast<std::vector<Handler> &>(deferredHandlers);
        typename std::vector<Handler>::iterator it = ref.begin();
        for (; it != ref.end(); ++it)
          if (priority > it->priority)
            break;
        ref.insert(it, h);
      }
      virtual void deferUnbind(OnChangeFn cb, void *userData) const
      {
        Handler target = {cb, userData, false, 0};
        std::vector<Handler> &ref = const_cast<std::vector<Handler> &>(deferredHandlers);
        for (size_t i = 0; i < ref.size(); ++i)
        {
          if (ref[i] == target)
          {
            ref.erase(ref.begin() + i);
            break;
          }
        }
      }

      typedef void (*OnChangeWithOldFn)(T oldValue, T newValue, void *userData);
      void deferBindWithOld(OnChangeWithOldFn cb, void *userData, int priority = 0) const
      {
        OldNewCtx *ctx = new OldNewCtx();
        ctx->lastValue = this->get();
        ctx->cb = cb;
        ctx->userData = userData;
        ctx->state = this;
        this->deferBind(&OldNewThunk, ctx, priority);
      }

    protected:
      virtual void set(const T &v)
      {
        if (!stateValuesEqual(value, v))
        {
          value = v;
          notifyStateChanged();
        }
      }
      virtual void setValue(const T &v) { set(v); }
      // setValue(const ValueHolderBase&) removed as no longer needed
      // Notify
      void notifyStateChanged()
      {
        for (size_t i = 0; i < handlers.size();)
        {
          handlers[i].cb(handlers[i].userData);
          if (handlers[i].callOnce)
            handlers.erase(handlers.begin() + i);
          else
            ++i;
        }
        // deferBindで登録されたハンドラも呼ぶ
        for (size_t i = 0; i < deferredHandlers.size(); ++i)
        {
          deferredHandlers[i].cb(deferredHandlers[i].userData);
        }
      }

    protected:
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
      std::vector<Handler> deferredHandlers;
      T value;

    private:
      struct OldNewCtx
      {
        T lastValue;
        OnChangeWithOldFn cb;
        void *userData;
        const State<T> *state;
      };
      static void OldNewThunk(void *ud)
      {
        OldNewCtx *ctx = static_cast<OldNewCtx *>(ud);
        T newValue = ctx->state->get();
        if (!stateValuesEqual(ctx->lastValue, newValue))
        {
          ctx->cb(ctx->lastValue, newValue, ctx->userData);
          ctx->lastValue = newValue;
        }
      }
    };

    // State<void> specialization: No value held, event propagation only
    // - Generic State for expressing UI events (button clicks, notifications, triggers, etc.) or "state changes without value"
    // - Also used as base for event-based States like EmitterState
    // - Realizes "type-safe event propagation" everywhere: tests, app body, extension libraries, etc.
    // Has the same bind API and handler management as State<T>
    //
    template <>
    class State<void> : public StateBase
    {
    public:
      State() {}
      virtual ~State() {}

      virtual void bind(OnChangeFn cb, void *userData, bool callImmediately = true, bool callOnce = false, int priority = 0)
      {
        Handler h = {cb, userData, callOnce, priority};
        std::vector<Handler>::iterator it = handlers.begin();
        for (; it != handlers.end(); ++it)
          if (priority > it->priority)
            break;
        handlers.insert(it, h);
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
        for (size_t i = 0; i < handlers.size(); ++i)
        {
          if (handlers[i] == target)
          {
            handlers.erase(handlers.begin() + i);
            break;
          }
        }
      }
      virtual void deferBind(OnChangeFn cb, void *userData, int priority = 0) const
      {
        Handler h = {cb, userData, false, priority};
        std::vector<Handler> &ref = const_cast<std::vector<Handler> &>(deferredHandlers);
        std::vector<Handler>::iterator it = ref.begin();
        for (; it != ref.end(); ++it)
          if (priority > it->priority)
            break;
        ref.insert(it, h);
      }
      virtual void deferUnbind(OnChangeFn cb, void *userData) const
      {
        Handler target = {cb, userData, false, 0};
        std::vector<Handler> &ref = const_cast<std::vector<Handler> &>(deferredHandlers);
        for (size_t i = 0; i < ref.size(); ++i)
        {
          if (ref[i] == target)
          {
            ref.erase(ref.begin() + i);
            break;
          }
        }
      }

    protected:
      // Event notification API - Used by derived classes like EmitterState
      void notifyStateChanged()
      {
        for (size_t i = 0; i < handlers.size();)
        {
          handlers[i].cb(handlers[i].userData);
          if (handlers[i].callOnce)
            handlers.erase(handlers.begin() + i);
          else
            ++i;
        }
        // Also call handlers registered with deferBind
        for (size_t i = 0; i < deferredHandlers.size(); ++i)
        {
          deferredHandlers[i].cb(deferredHandlers[i].userData);
        }
      }

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
      std::vector<Handler> deferredHandlers;
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
    template <typename T>
    class MutableState : public State<T>
    {
    public:
      MutableState() : State<T>() {}
      explicit MutableState(const T &initial) : State<T>(initial) {}
      virtual void *asMutableState() { return this; }
      using State<T>::setValue;
      void set(const T &v, bool forceUpdate = false)
      {
#ifdef TEST_BUILD
        printf("[MutableState::set] this=%p\n", (void *)this);
#endif
        if (forceUpdate)
        {
          State<T>::set(v);
          this->notifyStateChanged(); // 値が同じでも必ず通知
        }
        else
        {
          State<T>::set(v);
        }
        if (this->currentTracker)
          this->currentTracker->markDirty(this);
      }
    };

    // --- DerivedState: C++98-compatible implementation of State<T> ---
    template <typename T>
    class DerivedState : public State<T>
    {
    public:
      // Pure virtual base class for evaluation expression (C++98-compatible: operator() can be const reference)
      struct EvalFn
      {
        virtual ~EvalFn() {}
        virtual T operator()() = 0;
      };
      DerivedState(const std::vector<StateBase *> &deps, EvalFn *eval)
          : State<T>(eval ? (*eval)() : T()), dependencies(deps), evalFn(eval)
      {
        this->value = evalFn ? (*evalFn)() : T();
      }
      DerivedState(StateBase *dep, EvalFn *eval)
          : State<T>(eval ? (*eval)() : T()), evalFn(eval)
      {
        if (dep)
          dependencies.push_back(dep);
        this->value = evalFn ? (*evalFn)() : T();
      }
      DerivedState(StateBase *dep1, StateBase *dep2, EvalFn *eval)
          : State<T>(eval ? (*eval)() : T()), evalFn(eval)
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

    // Utility for creating static instances of State<T>
    // Example: auto* s = StaticState<int>(42);
    template <typename T>
    static State<T> *StaticState(const T &value)
    {
      static State<T> s(value);
      return &s;
    }

  } // namespace core
} // namespace loka

#endif // LOKA_STATE_HPP
