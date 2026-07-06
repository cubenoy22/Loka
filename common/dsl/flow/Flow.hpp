#ifndef LOKA_DSL_FLOW_HPP
#define LOKA_DSL_FLOW_HPP

#include <cassert>
#include <vector>

#include "core/State.hpp"
#include "core/util/StateTrackerGuard.hpp"

namespace loka
{
  namespace dsl
  {
    enum StepRunStatus
    {
      FLOW_STEP_PENDING = 0,
      FLOW_STEP_SUCCEEDED = 1,
      FLOW_STEP_FAILED = 2
    };

    enum FlowRunResult
    {
      FLOW_RUN_PENDING = 0,
      FLOW_RUN_SUCCEEDED = 1,
      FLOW_RUN_FAILED = 2,
      FLOW_RUN_CANCELED = 3
    };

    struct FlowError
    {
      int kind;
      int code;
      FlowError()
          : kind(0),
            code(0)
      {
      }
    };

    enum
    {
      FLOW_ERROR_KIND_FLOW = 1000
    };

    enum
    {
      FLOW_ERROR_CODE_STEP_PENDING_TIMEOUT = 1001,
      FLOW_ERROR_CODE_ASSERT_PREDICATE_FAILED = 1002
    };

    enum FlowHandleResult
    {
      FLOW_ERROR_UNHANDLED = 0,
      FLOW_ERROR_HANDLED = 1
    };

    namespace flow_detail
    {
      template <typename A, typename B> struct IsSame
      {
        enum
        {
          value = 0
        };
      };

      template <typename A> struct IsSame<A, A>
      {
        enum
        {
          value = 1
        };
      };
    } // namespace flow_detail

    struct MutableStateBinding
    {
      void *statePtr;
      void (*setter)(void *statePtr, const void *valuePtr);
    };

    template <typename T> static void MutableStateSetter(void *ptr, const void *val)
    {
      static_cast<loka::core::MutableState<T> *>(ptr)->set(*static_cast<const T *>(val), true);
    }

    struct MutableStateFieldBinding
    {
      void *statePtr;
      void (*setter)(void *statePtr, const void *outputPtr, std::size_t offset);
      std::size_t offset;
    };

    template <typename FieldT>
    static void MutableStateFieldSetter(void *statePtr, const void *outputPtr, std::size_t offset)
    {
      const char *base = static_cast<const char *>(outputPtr);
      const FieldT *field = reinterpret_cast<const FieldT *>(base + offset);
      static_cast<loka::core::MutableState<FieldT> *>(statePtr)->set(*field, true);
    }

    template <typename AdapterT> class StepSpec
    {
    public:
      typedef typename AdapterT::In In;
      typedef typename AdapterT::Out Out;
      typedef void (*SuccessFn)(const Out &, void *);
      typedef void (*FinallyFn)(void *);
      typedef bool (*ErrorMatcherFn)(const FlowError &, void *);
      typedef FlowHandleResult (*ErrorHandlerFn)(const FlowError &, void *);

      struct SuccessCallback
      {
        SuccessFn fn;
        void *user;
        int resumeStepId;
      };

      struct FailureCallback
      {
        ErrorMatcherFn matcher;
        ErrorHandlerFn handler;
        void *user;
        int resumeStepId;
      };

      explicit StepSpec(int id, const AdapterT &adapter)
          : id_(id),
            adapter_(adapter),
            input_(0),
            finallyFn_(0),
            finallyUser_(0),
            pendingTimeout_(-1)
      {
      }

      StepSpec &input(const In *value)
      {
        this->input_ = value;
        return *this;
      }

      StepSpec &onSuccess(SuccessFn fn, void *user)
      {
        SuccessCallback cb;
        cb.fn = fn;
        cb.user = user;
        cb.resumeStepId = -1;
        this->successCallbacks_.push_back(cb);
        return *this;
      }

      StepSpec &onSuccess(SuccessFn fn, void *user, int resumeStepId)
      {
        SuccessCallback cb;
        cb.fn = fn;
        cb.user = user;
        cb.resumeStepId = resumeStepId;
        this->successCallbacks_.push_back(cb);
        return *this;
      }

      StepSpec &onSuccess(Out *targetState)
      {
        this->successStates_.push_back(targetState);
        return *this;
      }

      StepSpec &onSuccess(Out *targetState, int resumeStepId)
      {
        this->successStates_.push_back(targetState);
        SuccessCallback cb;
        cb.fn = 0;
        cb.user = 0;
        cb.resumeStepId = resumeStepId;
        this->successCallbacks_.push_back(cb);
        return *this;
      }

      StepSpec &onSuccess(loka::core::MutableState<Out> *state)
      {
        MutableStateBinding binding;
        binding.statePtr = state;
        binding.setter = &MutableStateSetter<Out>;
        this->mutableStateBindings_.push_back(binding);
        return *this;
      }

      template <typename FieldT, typename ClassT>
      StepSpec &onSuccess(loka::core::MutableState<FieldT> *state, FieldT ClassT::*member)
      {
        typedef char LokaFieldBindingTypeMismatch[(flow_detail::IsSame<Out, ClassT>::value) ? 1 : -1];
        (void)sizeof(LokaFieldBindingTypeMismatch);
        MutableStateFieldBinding binding;
        binding.statePtr = state;
        binding.setter = &MutableStateFieldSetter<FieldT>;
        Out temp = Out();
        binding.offset = static_cast<std::size_t>(reinterpret_cast<const char *>(&(temp.*member))
                                                  - reinterpret_cast<const char *>(&temp));
        this->fieldBindings_.push_back(binding);
        return *this;
      }

      StepSpec &onFailure(ErrorMatcherFn matcher, ErrorHandlerFn handler, void *user)
      {
        FailureCallback cb;
        cb.matcher = matcher;
        cb.handler = handler;
        cb.user = user;
        cb.resumeStepId = -1;
        this->failureCallbacks_.push_back(cb);
        return *this;
      }

      StepSpec &onFailure(ErrorMatcherFn matcher, ErrorHandlerFn handler, void *user, int resumeStepId)
      {
        FailureCallback cb;
        cb.matcher = matcher;
        cb.handler = handler;
        cb.user = user;
        cb.resumeStepId = resumeStepId;
        this->failureCallbacks_.push_back(cb);
        return *this;
      }

      StepSpec &onFailure(ErrorHandlerFn handler, void *user)
      {
        return this->onFailure(&StepSpec::alwaysMatch, handler, user);
      }

      StepSpec &onFailure(ErrorHandlerFn handler, void *user, int resumeStepId)
      {
        return this->onFailure(&StepSpec::alwaysMatch, handler, user, resumeStepId);
      }

      StepSpec &onFinally(FinallyFn fn, void *user)
      {
        this->finallyFn_ = fn;
        this->finallyUser_ = user;
        return *this;
      }

      StepSpec &timeoutPending(int maxPendingRuns)
      {
        this->pendingTimeout_ = maxPendingRuns;
        return *this;
      }

      static bool alwaysMatch(const FlowError &, void *)
      {
        return true;
      }

      int id() const
      {
        return this->id_;
      }

      const AdapterT &adapter() const
      {
        return this->adapter_;
      }

      const In *inputPtr() const
      {
        return this->input_;
      }

      const std::vector<SuccessCallback> &successCallbacks() const
      {
        return this->successCallbacks_;
      }

      const std::vector<Out *> &successStates() const
      {
        return this->successStates_;
      }

      const std::vector<FailureCallback> &failureCallbacks() const
      {
        return this->failureCallbacks_;
      }

      const std::vector<MutableStateBinding> &mutableStateBindings() const
      {
        return this->mutableStateBindings_;
      }

      const std::vector<MutableStateFieldBinding> &fieldBindings() const
      {
        return this->fieldBindings_;
      }

      FinallyFn finallyFn() const
      {
        return this->finallyFn_;
      }

      void *finallyUser() const
      {
        return this->finallyUser_;
      }

      int pendingTimeout() const
      {
        return this->pendingTimeout_;
      }

    private:
      int id_;
      AdapterT adapter_;
      const In *input_;
      std::vector<SuccessCallback> successCallbacks_;
      std::vector<Out *> successStates_;
      std::vector<FailureCallback> failureCallbacks_;
      std::vector<MutableStateBinding> mutableStateBindings_;
      std::vector<MutableStateFieldBinding> fieldBindings_;
      FinallyFn finallyFn_;
      void *finallyUser_;
      int pendingTimeout_;
    };

    template <typename AdapterT> StepSpec<AdapterT> Step(int id, const AdapterT &adapter)
    {
      return StepSpec<AdapterT>(id, adapter);
    }

    template <typename T> class AssertPredicateAdapter
    {
    public:
      typedef T In;
      typedef T Out;
      typedef bool (*PredicateFn)(const T &, void *);

      AssertPredicateAdapter(PredicateFn predicate,
                             void *user,
                             int errorKind = FLOW_ERROR_KIND_FLOW,
                             int errorCode = FLOW_ERROR_CODE_ASSERT_PREDICATE_FAILED)
          : predicate_(predicate),
            user_(user),
            errorKind_(errorKind),
            errorCode_(errorCode)
      {
      }

      StepRunStatus run(const T &in, T &out, FlowError &error) const
      {
        out = in;
        if (!this->predicate_ || this->predicate_(in, this->user_))
        {
          return FLOW_STEP_SUCCEEDED;
        }
        error.kind = this->errorKind_;
        error.code = this->errorCode_;
        return FLOW_STEP_FAILED;
      }

    private:
      PredicateFn predicate_;
      void *user_;
      int errorKind_;
      int errorCode_;
    };

    template <typename T>
    AssertPredicateAdapter<T> AssertPredicate(typename AssertPredicateAdapter<T>::PredicateFn predicate,
                                              void *user,
                                              int errorKind = FLOW_ERROR_KIND_FLOW,
                                              int errorCode = FLOW_ERROR_CODE_ASSERT_PREDICATE_FAILED)
    {
      return AssertPredicateAdapter<T>(predicate, user, errorKind, errorCode);
    }

    class FlowChainImpl
    {
    public:
      FlowChainImpl()
          : finallyFn_(0),
            finallyUser_(0),
            loadingState_(0),
            tracker_(0),
            triggerState_(0),
            triggerInputBuffer_(0),
            triggerReadFn_(0),
            triggerDeleteFn_(0),
            triggerCallback_(0),
            triggerRunning_(false),
            cancelRequested_(false),
            runBeginFn_(0),
            runEndFn_(0),
            runHookFlow_(0),
            runHookUser_(0),
            refs_(1),
            runPins_(0)
      {
      }

      ~FlowChainImpl()
      {
        this->clearTrigger();
        for (std::size_t i = 0; i < this->steps_.size(); ++i)
        {
          delete this->steps_[i];
        }
      }

      bool isShared() const
      {
        return this->refs_ > 1;
      }

      void retain()
      {
        ++this->refs_;
      }

      void release()
      {
        --this->refs_;
        if (this->refs_ == 0 && this->runPins_ == 0)
        {
          delete this;
        }
      }

      struct FlowSuccessCallback
      {
        void (*fn)(void *);
        void *user;
        int resumeStepId;
      };

      struct FlowFailureCallback
      {
        bool (*matcher)(const FlowError &, void *);
        FlowHandleResult (*handler)(const FlowError &, void *);
        void *user;
        int resumeStepId;
      };

      typedef void (*RunLifecycleFn)(void *, void *);

      std::vector<FlowSuccessCallback> successCallbacks_;
      std::vector<FlowFailureCallback> failureCallbacks_;
      void (*finallyFn_)(void *);
      void *finallyUser_;
      bool *loadingState_;
      loka::core::PushStateTracker *tracker_;

      // Trigger state members
      loka::core::StateBase *triggerState_;
      void *triggerInputBuffer_;
      void (*triggerReadFn_)(void *buffer, loka::core::StateBase *state);
      void (*triggerDeleteFn_)(void *buffer);
      loka::core::StateBase::OnChangeFn triggerCallback_;
      bool triggerRunning_;
      mutable bool cancelRequested_;
      RunLifecycleFn runBeginFn_;
      RunLifecycleFn runEndFn_;
      void *runHookFlow_;
      void *runHookUser_;

      class IRuntimeStep
      {
      public:
        virtual ~IRuntimeStep() {}
        virtual IRuntimeStep *clone() const = 0;
        virtual int stepId() const = 0;
        virtual StepRunStatus run(const void *inputFromPrev,
                                  FlowError &error,
                                  bool &errorHandled,
                                  int &resumeStepId,
                                  int &successResumeStepId) = 0;
        virtual const void *outputPtr() const = 0;
      };

      std::vector<IRuntimeStep *> steps_;

      FlowChainImpl *clone() const
      {
        FlowChainImpl *next = new FlowChainImpl();
        next->successCallbacks_ = this->successCallbacks_;
        next->failureCallbacks_ = this->failureCallbacks_;
        next->finallyFn_ = this->finallyFn_;
        next->finallyUser_ = this->finallyUser_;
        next->loadingState_ = this->loadingState_;
        next->tracker_ = this->tracker_;
        next->cancelRequested_ = this->cancelRequested_;
        // trigger is NOT cloned
        for (std::size_t i = 0; i < this->steps_.size(); ++i)
        {
          next->steps_.push_back(this->steps_[i]->clone());
        }
        return next;
      }

      // --- Methods moved from FlowChain ---

      void terminalCleanup() const
      {
        if (this->tracker_ != 0)
        {
          this->tracker_->end();
        }
        if (this->finallyFn_ != 0)
        {
          this->finallyFn_(this->finallyUser_);
        }
        if (this->loadingState_ != 0)
        {
          *this->loadingState_ = false;
        }
      }

      bool findStepIndex(int stepId, std::size_t &indexOut) const
      {
        for (std::size_t i = 0; i < this->steps_.size(); ++i)
        {
          if (this->steps_[i]->stepId() == stepId)
          {
            indexOut = i;
            return true;
          }
        }
        return false;
      }

      FlowRunResult runPinnedFromIndex(std::size_t startIndex) const
      {
        RunPinScope runPinScope(this);
        return this->runCoreFromIndex(startIndex);
      }

      FlowRunResult runPinnedFromStepId(int stepId) const
      {
        std::size_t start = 0;
        if (!this->findStepIndex(stepId, start))
        {
          return FLOW_RUN_FAILED;
        }
        return this->runPinnedFromIndex(start);
      }

      FlowRunResult runCoreFromIndex(std::size_t startIndex) const
      {
        // Snapshot the hook fields once: begin/end stay paired even if the
        // hooks are cleared mid-run (e.g. the owning FlowSlot disowns the
        // flow while a shared-impl copy is executing).
        struct RunScope
        {
          explicit RunScope(const FlowChainImpl *impl)
              : beginFn_(impl->runBeginFn_),
                endFn_(impl->runEndFn_),
                hookFlow_(impl->runHookFlow_),
                hookUser_(impl->runHookUser_)
          {
            assert((this->beginFn_ == 0) == (this->endFn_ == 0)
                   && "FlowChainImpl::runFromIndex requires begin/end hooks to be paired");
            if (this->beginFn_ != 0)
            {
              assert(this->hookFlow_ != 0 && "FlowChainImpl::runFromIndex requires a hook flow");
              this->beginFn_(this->hookFlow_, this->hookUser_);
            }
          }

          ~RunScope()
          {
            if (this->beginFn_ != 0 && this->endFn_ != 0)
            {
              this->endFn_(this->hookFlow_, this->hookUser_);
            }
          }

          RunLifecycleFn beginFn_;
          RunLifecycleFn endFn_;
          void *hookFlow_;
          void *hookUser_;
        } runScope(this);

        if (startIndex >= this->steps_.size())
        {
          return FLOW_RUN_FAILED;
        }

        const void *current = 0;
        if (startIndex == 0 && this->triggerInputBuffer_)
        {
          current = this->triggerInputBuffer_;
        }
        else if (startIndex > 0 && startIndex < this->steps_.size())
        {
          current = this->steps_[startIndex - 1]->outputPtr();
        }
        FlowError error;
        if (this->tracker_ != 0)
        {
          this->tracker_->begin();
        }
        if (this->loadingState_ != 0)
        {
          *this->loadingState_ = true;
        }

        static const std::size_t MAX_ITERATIONS = 1024;
        std::size_t iterations = 0;
        for (std::size_t i = startIndex; i < this->steps_.size(); ++i)
        {
          if (this->cancelRequested_)
          {
            this->cancelRequested_ = false;
            this->terminalCleanup();
            return FLOW_RUN_CANCELED;
          }
          if (++iterations > MAX_ITERATIONS)
          {
            this->terminalCleanup();
            return FLOW_RUN_FAILED;
          }
          bool stepHandled = false;
          int stepResumeStepId = -1;
          int stepSuccessResumeStepId = -1;
          StepRunStatus stepStatus =
              this->steps_[i]->run(current, error, stepHandled, stepResumeStepId, stepSuccessResumeStepId);
          if (stepStatus == FLOW_STEP_SUCCEEDED)
          {
            current = this->steps_[i]->outputPtr();

            if (stepSuccessResumeStepId >= 0)
            {
              std::size_t jumpIndex = 0;
              if (!this->findStepIndex(stepSuccessResumeStepId, jumpIndex))
              {
                this->terminalCleanup();
                return FLOW_RUN_FAILED;
              }
              i = (jumpIndex == 0) ? static_cast<std::size_t>(-1) : (jumpIndex - 1);
            }
            continue;
          }

          if (stepStatus == FLOW_STEP_PENDING)
          {
            if (this->tracker_ != 0)
            {
              this->tracker_->end();
            }
            return FLOW_RUN_PENDING;
          }

          bool flowHandled = false;
          int flowResumeStepId = -1;
          for (std::size_t k = 0; k < this->failureCallbacks_.size(); ++k)
          {
            const FlowFailureCallback &cb = this->failureCallbacks_[k];
            if (cb.matcher != 0 && cb.matcher(error, cb.user))
            {
              const bool handled = (cb.handler != 0 && cb.handler(error, cb.user) == FLOW_ERROR_HANDLED);
              flowHandled = handled;
              if (handled && cb.resumeStepId >= 0)
              {
                flowResumeStepId = cb.resumeStepId;
              }
              break; // first-match-wins
            }
          }

          const int jumpStepId = stepResumeStepId >= 0 ? stepResumeStepId : flowResumeStepId;
          if (jumpStepId >= 0)
          {
            std::size_t jumpIndex = 0;
            if (!this->findStepIndex(jumpStepId, jumpIndex))
            {
              this->terminalCleanup();
              return FLOW_RUN_FAILED;
            }
            i = (jumpIndex == 0) ? static_cast<std::size_t>(-1) : (jumpIndex - 1);
            continue;
          }

          this->terminalCleanup();
          return stepHandled || flowHandled ? FLOW_RUN_SUCCEEDED : FLOW_RUN_FAILED;
        }

        // Flow completed successfully — fire flow-level success callbacks.
        int flowSuccessResumeStepId = -1;
        for (std::size_t k = 0; k < this->successCallbacks_.size(); ++k)
        {
          this->successCallbacks_[k].fn(this->successCallbacks_[k].user);
          if (flowSuccessResumeStepId < 0 && this->successCallbacks_[k].resumeStepId >= 0)
          {
            flowSuccessResumeStepId = this->successCallbacks_[k].resumeStepId;
          }
        }

        if (flowSuccessResumeStepId >= 0)
        {
          std::size_t jumpIndex = 0;
          if (!this->findStepIndex(flowSuccessResumeStepId, jumpIndex))
          {
            this->terminalCleanup();
            return FLOW_RUN_FAILED;
          }
          this->terminalCleanup();
          return this->runCoreFromIndex(jumpIndex);
        }

        this->terminalCleanup();
        return FLOW_RUN_SUCCEEDED;
      }

      // --- Trigger support ---

      void setTrigger(loka::core::StateBase *state,
                      void *buffer,
                      void (*readFn)(void *, loka::core::StateBase *),
                      void (*deleteFn)(void *),
                      loka::core::StateBase::OnChangeFn callback)
      {
        this->clearTrigger();
        this->triggerState_ = state;
        this->triggerInputBuffer_ = buffer;
        this->triggerReadFn_ = readFn;
        this->triggerDeleteFn_ = deleteFn;
        this->triggerCallback_ = callback;
        this->triggerRunning_ = false;
        state->deferBind(callback, this);
      }

      void clearTrigger()
      {
        if (this->triggerState_ && this->triggerCallback_)
        {
          this->triggerState_->deferUnbind(this->triggerCallback_, this);
        }
        if (this->triggerDeleteFn_ && this->triggerInputBuffer_)
        {
          this->triggerDeleteFn_(this->triggerInputBuffer_);
        }
        this->triggerState_ = 0;
        this->triggerInputBuffer_ = 0;
        this->triggerReadFn_ = 0;
        this->triggerDeleteFn_ = 0;
        this->triggerCallback_ = 0;
        this->triggerRunning_ = false;
      }

      static void OnTriggerChanged(void *userData)
      {
        FlowChainImpl *self = static_cast<FlowChainImpl *>(userData);
        if (self->triggerRunning_)
          return;
        RunPinScope runPinScope(self);
        self->triggerRunning_ = true;
        if (self->triggerReadFn_ && self->triggerInputBuffer_)
        {
          self->triggerReadFn_(self->triggerInputBuffer_, self->triggerState_);
        }
        // Trigger callbacks hold only the impl pointer, so lifetime still
        // needs pinning here; use the run pin rather than refs_ so callback
        // code can cancel the active impl without tripping copy-on-write.
        FlowRunResult result = self->runCoreFromIndex(0);
        if (result != FLOW_RUN_PENDING)
        {
          self->triggerRunning_ = false;
        }
      }

    private:
      void pinRun()
      {
        ++this->runPins_;
      }

      void unpinRun()
      {
        assert(this->runPins_ > 0 && "FlowChainImpl::unpinRun underflow");
        --this->runPins_;
        if (this->refs_ == 0 && this->runPins_ == 0)
        {
          delete this;
        }
      }

      struct RunPinScope
      {
        explicit RunPinScope(const FlowChainImpl *impl)
            : impl_(const_cast<FlowChainImpl *>(impl))
        {
          assert(this->impl_ != 0 && "FlowChainImpl::RunPinScope requires impl");
          this->impl_->pinRun();
        }

        ~RunPinScope()
        {
          this->impl_->unpinRun();
        }

        FlowChainImpl *impl_;

      private:
        RunPinScope(const RunPinScope &);
        RunPinScope &operator=(const RunPinScope &);
      };

      int refs_;
      int runPins_;
      FlowChainImpl(const FlowChainImpl &);
      FlowChainImpl &operator=(const FlowChainImpl &);
    };

    template <typename AdapterT> class RuntimeStep : public FlowChainImpl::IRuntimeStep
    {
    public:
      typedef StepSpec<AdapterT> Spec;
      typedef typename Spec::In In;
      typedef typename Spec::Out Out;

      explicit RuntimeStep(const Spec &spec)
          : spec_(spec),
            out_(),
            pendingCount_(0)
      {
      }

      virtual FlowChainImpl::IRuntimeStep *clone() const
      {
        return new RuntimeStep<AdapterT>(this->spec_);
      }

      virtual int stepId() const
      {
        return this->spec_.id();
      }

      virtual StepRunStatus
      run(const void *inputFromPrev, FlowError &error, bool &errorHandled, int &resumeStepId, int &successResumeStepId)
      {
        errorHandled = false;
        resumeStepId = -1;
        successResumeStepId = -1;
        const In *in = this->spec_.inputPtr();
        if (in == 0)
        {
          in = static_cast<const In *>(inputFromPrev);
        }
        assert(in != 0);

        StepRunStatus status = this->spec_.adapter().run(*in, this->out_, error);
        if (status == FLOW_STEP_SUCCEEDED)
        {
          this->pendingCount_ = 0;
          const std::vector<typename Spec::SuccessCallback> &callbacks = this->spec_.successCallbacks();
          for (std::size_t i = 0; i < callbacks.size(); ++i)
          {
            if (callbacks[i].fn != 0)
            {
              callbacks[i].fn(this->out_, callbacks[i].user);
            }
            if (successResumeStepId < 0 && callbacks[i].resumeStepId >= 0)
            {
              successResumeStepId = callbacks[i].resumeStepId;
            }
          }

          const std::vector<Out *> &states = this->spec_.successStates();
          for (std::size_t i = 0; i < states.size(); ++i)
          {
            if (states[i] != 0)
            {
              *states[i] = this->out_;
            }
          }

          const std::vector<MutableStateBinding> &bindings = this->spec_.mutableStateBindings();
          for (std::size_t i = 0; i < bindings.size(); ++i)
          {
            bindings[i].setter(bindings[i].statePtr, &this->out_);
          }

          const std::vector<MutableStateFieldBinding> &fields = this->spec_.fieldBindings();
          for (std::size_t i = 0; i < fields.size(); ++i)
          {
            fields[i].setter(fields[i].statePtr, &this->out_, fields[i].offset);
          }

          if (this->spec_.finallyFn() != 0)
          {
            this->spec_.finallyFn()(this->spec_.finallyUser());
          }
          return FLOW_STEP_SUCCEEDED;
        }

        if (status == FLOW_STEP_PENDING)
        {
          ++this->pendingCount_;
          if (this->spec_.pendingTimeout() >= 0 && this->pendingCount_ > this->spec_.pendingTimeout())
          {
            error.kind = FLOW_ERROR_KIND_FLOW;
            error.code = FLOW_ERROR_CODE_STEP_PENDING_TIMEOUT;
            status = FLOW_STEP_FAILED;
          }
          else
          {
            return FLOW_STEP_PENDING;
          }
        }

        this->pendingCount_ = 0;

        const std::vector<typename Spec::FailureCallback> &failureCallbacks = this->spec_.failureCallbacks();
        this->pendingCount_ = 0;
        for (std::size_t i = 0; i < failureCallbacks.size(); ++i)
        {
          const typename Spec::FailureCallback &cb = failureCallbacks[i];
          if (cb.matcher != 0 && cb.matcher(error, cb.user))
          {
            const bool handled = (cb.handler != 0 && cb.handler(error, cb.user) == FLOW_ERROR_HANDLED);
            errorHandled = handled;
            if (handled && cb.resumeStepId >= 0)
            {
              resumeStepId = cb.resumeStepId;
            }
            break; // first-match-wins
          }
        }

        if (this->spec_.finallyFn() != 0)
        {
          this->spec_.finallyFn()(this->spec_.finallyUser());
        }
        return FLOW_STEP_FAILED;
      }

      virtual const void *outputPtr() const
      {
        return &this->out_;
      }

    private:
      Spec spec_;
      Out out_;
      int pendingCount_;
    };

    template <typename InT, typename OutT> class FlowChain
    {
    public:
      typedef InT In;
      typedef OutT Out;
      typedef bool (*ErrorMatcherFn)(const FlowError &, void *);
      typedef FlowHandleResult (*ErrorHandlerFn)(const FlowError &, void *);

      explicit FlowChain(FlowChainImpl *impl)
          : impl_(impl)
      {
      }

      typedef typename FlowChainImpl::RunLifecycleFn RunLifecycleFn;

      FlowChain(const FlowChain &other)
          : impl_(other.impl_)
      {
        this->impl_->retain();
      }

      ~FlowChain()
      {
        this->impl_->release();
      }

      FlowChain &operator=(const FlowChain &other)
      {
        if (this != &other)
        {
          this->impl_->release();
          this->impl_ = other.impl_;
          this->impl_->retain();
        }
        return *this;
      }

      template <typename NextAdapterT>
      FlowChain<InT, typename NextAdapterT::Out> operator|(const StepSpec<NextAdapterT> &step) const
      {
        typedef typename NextAdapterT::In NextIn;
        typedef typename NextAdapterT::Out NextOut;
        typedef char LokaFlowStepTypeMismatch[(flow_detail::IsSame<OutT, NextIn>::value) ? 1 : -1];
        (void)sizeof(LokaFlowStepTypeMismatch);

        FlowChainImpl *nextImpl = this->impl_->clone();
        nextImpl->steps_.push_back(new RuntimeStep<NextAdapterT>(step));
        return FlowChain<InT, NextOut>(nextImpl);
      }

      FlowChain &onSuccess(void (*fn)(void *), void *user)
      {
        this->detachIfShared();
        FlowChainImpl::FlowSuccessCallback cb;
        cb.fn = fn;
        cb.user = user;
        cb.resumeStepId = -1;
        this->impl_->successCallbacks_.push_back(cb);
        return *this;
      }

      FlowChain &onSuccess(void (*fn)(void *), void *user, int resumeStepId)
      {
        this->detachIfShared();
        FlowChainImpl::FlowSuccessCallback cb;
        cb.fn = fn;
        cb.user = user;
        cb.resumeStepId = resumeStepId;
        this->impl_->successCallbacks_.push_back(cb);
        return *this;
      }

      FlowChain &onFailure(ErrorMatcherFn matcher, ErrorHandlerFn handler, void *user)
      {
        this->detachIfShared();
        FlowChainImpl::FlowFailureCallback cb;
        cb.matcher = matcher;
        cb.handler = handler;
        cb.user = user;
        cb.resumeStepId = -1;
        this->impl_->failureCallbacks_.push_back(cb);
        return *this;
      }

      FlowChain &onFailure(ErrorMatcherFn matcher, ErrorHandlerFn handler, void *user, int resumeStepId)
      {
        this->detachIfShared();
        FlowChainImpl::FlowFailureCallback cb;
        cb.matcher = matcher;
        cb.handler = handler;
        cb.user = user;
        cb.resumeStepId = resumeStepId;
        this->impl_->failureCallbacks_.push_back(cb);
        return *this;
      }

      FlowChain &onFailure(ErrorHandlerFn handler, void *user)
      {
        return this->onFailure(&FlowChain::alwaysMatch, handler, user);
      }

      FlowChain &onFailure(ErrorHandlerFn handler, void *user, int resumeStepId)
      {
        return this->onFailure(&FlowChain::alwaysMatch, handler, user, resumeStepId);
      }

      FlowChain &onFinally(void (*fn)(void *), void *user)
      {
        this->detachIfShared();
        this->impl_->finallyFn_ = fn;
        this->impl_->finallyUser_ = user;
        return *this;
      }

      FlowChain &trackLoading(bool *loadingState)
      {
        this->detachIfShared();
        this->impl_->loadingState_ = loadingState;
        return *this;
      }

      FlowChain &withTracker(loka::core::PushStateTracker *tracker)
      {
        this->detachIfShared();
        this->impl_->tracker_ = tracker;
        return *this;
      }

      FlowChain &cancel()
      {
        this->detachIfShared();
        this->impl_->cancelRequested_ = true;
        return *this;
      }

      FlowChain &clearCancel()
      {
        this->detachIfShared();
        this->impl_->cancelRequested_ = false;
        return *this;
      }

      FlowChain &bindTrigger(loka::core::State<InT> *source)
      {
        this->detachIfShared();
        InT *buffer = new InT();
        this->impl_->setTrigger(
            source, buffer, &FlowChain::TriggerRead, &FlowChain::TriggerDelete, &FlowChainImpl::OnTriggerChanged);
        return *this;
      }

      bool run() const
      {
        const FlowRunResult result = this->impl_->runPinnedFromIndex(0);
        return result == FLOW_RUN_SUCCEEDED;
      }

      FlowRunResult runResult() const
      {
        return this->impl_->runPinnedFromIndex(0);
      }

      bool resume(int stepId) const
      {
        return this->resumeResult(stepId) == FLOW_RUN_SUCCEEDED;
      }

      FlowChain &setExecutionHooks(RunLifecycleFn beginFn, RunLifecycleFn endFn, void *user)
      {
        assert((beginFn == 0) == (endFn == 0) && "FlowChain::setExecutionHooks requires begin/end hooks to be paired");
        this->detachIfShared();
        this->impl_->runBeginFn_ = beginFn;
        this->impl_->runEndFn_ = endFn;
        this->impl_->runHookUser_ = user;
        this->impl_->runHookFlow_ = this;
        return *this;
      }

      // Clears the hooks on the SHARED impl without detaching: used when the
      // hook owner (FlowSlot) disowns this flow, so wrappers that still share
      // the impl cannot fire hooks into the owner's freed run state.
      void clearExecutionHooks()
      {
        this->impl_->runBeginFn_ = 0;
        this->impl_->runEndFn_ = 0;
        this->impl_->runHookFlow_ = 0;
        this->impl_->runHookUser_ = 0;
      }

      FlowRunResult resumeResult(int stepId) const
      {
        FlowChainImpl *impl = this->impl_;
        FlowRunResult result = impl->runPinnedFromStepId(stepId);
        if (result != FLOW_RUN_PENDING)
        {
          impl->triggerRunning_ = false;
        }
        return result;
      }

    public:
      static bool alwaysMatch(const FlowError &, void *)
      {
        return true;
      }

      void detachIfShared()
      {
        if (!this->impl_->isShared())
        {
          return;
        }
        FlowChainImpl *current = this->impl_;
        FlowChainImpl *next = current->clone();
        const RunLifecycleFn runBeginFn = current->runBeginFn_;
        const RunLifecycleFn runEndFn = current->runEndFn_;
        void *runHookUser = current->runHookUser_;
        const bool preserveHooks = (runBeginFn != 0 || runEndFn != 0) && current->runHookFlow_ == this;
        current->release();
        this->impl_ = next;
        if (preserveHooks)
        {
          this->impl_->runBeginFn_ = runBeginFn;
          this->impl_->runEndFn_ = runEndFn;
          this->impl_->runHookUser_ = runHookUser;
          this->impl_->runHookFlow_ = this;
        }
      }

      FlowChainImpl *impl_;

    private:
      static void TriggerRead(void *buf, loka::core::StateBase *st)
      {
        *static_cast<InT *>(buf) = static_cast<loka::core::State<InT> *>(st)->get();
      }

      static void TriggerDelete(void *buf)
      {
        delete static_cast<InT *>(buf);
      }
    };

    class Flow
    {
    public:
      template <typename AdapterT>
      FlowChain<typename AdapterT::In, typename AdapterT::Out> operator|(const StepSpec<AdapterT> &step) const
      {
        FlowChainImpl *impl = new FlowChainImpl();
        impl->finallyFn_ = 0;
        impl->finallyUser_ = 0;
        impl->steps_.push_back(new RuntimeStep<AdapterT>(step));
        return FlowChain<typename AdapterT::In, typename AdapterT::Out>(impl);
      }
    };
  } // namespace dsl
} // namespace loka

#endif // LOKA_DSL_FLOW_HPP
