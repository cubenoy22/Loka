#ifndef LOKA_DSL_FLOW_HPP
#define LOKA_DSL_FLOW_HPP

#include <cassert>
#include <vector>

namespace loka {
  namespace dsl {
    enum StepRunStatus {
      FLOW_STEP_PENDING = 0,
      FLOW_STEP_SUCCEEDED = 1,
      FLOW_STEP_FAILED = 2
    };

    enum FlowRunResult {
      FLOW_RUN_PENDING = 0,
      FLOW_RUN_SUCCEEDED = 1,
      FLOW_RUN_FAILED = 2
    };

    struct FlowError {
      int kind;
      int code;
      FlowError() : kind(0), code(0) {
      }
    };

    enum FlowHandleResult { FLOW_ERROR_UNHANDLED = 0, FLOW_ERROR_HANDLED = 1 };

    namespace flow_detail {
      template <typename A, typename B> struct IsSame {
        enum { value = 0 };
      };

      template <typename A> struct IsSame<A, A> {
        enum { value = 1 };
      };
    } // namespace flow_detail

    template <typename AdapterT> class StepSpec {
    public:
      typedef typename AdapterT::In In;
      typedef typename AdapterT::Out Out;
      typedef void (*SuccessFn)(const Out &, void *);
      typedef void (*FinallyFn)(void *);
      typedef bool (*ErrorMatcherFn)(const FlowError &, void *);
      typedef FlowHandleResult (*ErrorHandlerFn)(const FlowError &, void *);

      struct SuccessCallback {
        SuccessFn fn;
        void *user;
        int resumeStepId;
      };

      struct FailureCallback {
        ErrorMatcherFn matcher;
        ErrorHandlerFn handler;
        void *user;
        int resumeStepId;
      };

      explicit StepSpec(int id, const AdapterT &adapter)
          : id_(id), adapter_(adapter), input_(0), finallyFn_(0),
            finallyUser_(0) {
      }

      StepSpec &input(const In *value) {
        this->input_ = value;
        return *this;
      }

      StepSpec &onSuccess(SuccessFn fn, void *user) {
        SuccessCallback cb;
        cb.fn = fn;
        cb.user = user;
        cb.resumeStepId = -1;
        this->successCallbacks_.push_back(cb);
        return *this;
      }

      StepSpec &onSuccess(SuccessFn fn, void *user, int resumeStepId) {
        SuccessCallback cb;
        cb.fn = fn;
        cb.user = user;
        cb.resumeStepId = resumeStepId;
        this->successCallbacks_.push_back(cb);
        return *this;
      }

      StepSpec &onSuccess(Out *targetState) {
        this->successStates_.push_back(targetState);
        return *this;
      }

      StepSpec &onSuccess(Out *targetState, int resumeStepId) {
        this->successStates_.push_back(targetState);
        SuccessCallback cb;
        cb.fn = 0;
        cb.user = 0;
        cb.resumeStepId = resumeStepId;
        this->successCallbacks_.push_back(cb);
        return *this;
      }

      StepSpec &
      onFailure(ErrorMatcherFn matcher, ErrorHandlerFn handler, void *user) {
        FailureCallback cb;
        cb.matcher = matcher;
        cb.handler = handler;
        cb.user = user;
        cb.resumeStepId = -1;
        this->failureCallbacks_.push_back(cb);
        return *this;
      }

      StepSpec &onFailure(ErrorMatcherFn matcher, ErrorHandlerFn handler,
                          void *user, int resumeStepId) {
        FailureCallback cb;
        cb.matcher = matcher;
        cb.handler = handler;
        cb.user = user;
        cb.resumeStepId = resumeStepId;
        this->failureCallbacks_.push_back(cb);
        return *this;
      }

      StepSpec &onFailure(ErrorHandlerFn handler, void *user) {
        return this->onFailure(&StepSpec::alwaysMatch, handler, user);
      }

      StepSpec &onFailure(ErrorHandlerFn handler, void *user, int resumeStepId) {
        return this->onFailure(&StepSpec::alwaysMatch, handler, user, resumeStepId);
      }

      StepSpec &onFinally(FinallyFn fn, void *user) {
        this->finallyFn_ = fn;
        this->finallyUser_ = user;
        return *this;
      }

      static bool alwaysMatch(const FlowError &, void *) {
        return true;
      }

      int id() const {
        return this->id_;
      }

      const AdapterT &adapter() const {
        return this->adapter_;
      }

      const In *inputPtr() const {
        return this->input_;
      }

      const std::vector<SuccessCallback> &successCallbacks() const {
        return this->successCallbacks_;
      }

      const std::vector<Out *> &successStates() const {
        return this->successStates_;
      }

      const std::vector<FailureCallback> &failureCallbacks() const {
        return this->failureCallbacks_;
      }

      FinallyFn finallyFn() const {
        return this->finallyFn_;
      }

      void *finallyUser() const {
        return this->finallyUser_;
      }

    private:
      int id_;
      AdapterT adapter_;
      const In *input_;
      std::vector<SuccessCallback> successCallbacks_;
      std::vector<Out *> successStates_;
      std::vector<FailureCallback> failureCallbacks_;
      FinallyFn finallyFn_;
      void *finallyUser_;
    };

    template <typename AdapterT>
    StepSpec<AdapterT> Step(int id, const AdapterT &adapter) {
      return StepSpec<AdapterT>(id, adapter);
    }

    class FlowChainImpl {
    public:
      FlowChainImpl()
          : finallyFn_(0), finallyUser_(0), loadingState_(0), refs_(1) {
      }

      ~FlowChainImpl() {
        for (std::size_t i = 0; i < this->steps_.size(); ++i) {
          delete this->steps_[i];
        }
      }

      void retain() {
        ++this->refs_;
      }

      void release() {
        --this->refs_;
        if (this->refs_ == 0) {
          delete this;
        }
      }

      struct FlowSuccessCallback {
        void (*fn)(void *);
        void *user;
        int resumeStepId;
      };

      struct FlowFailureCallback {
        bool (*matcher)(const FlowError &, void *);
        FlowHandleResult (*handler)(const FlowError &, void *);
        void *user;
        int resumeStepId;
      };

      std::vector<FlowSuccessCallback> successCallbacks_;
      std::vector<FlowFailureCallback> failureCallbacks_;
      void (*finallyFn_)(void *);
      void *finallyUser_;
      bool *loadingState_;

      class IRuntimeStep {
      public:
        virtual ~IRuntimeStep() {
        }
        virtual IRuntimeStep *clone() const = 0;
        virtual int stepId() const = 0;
        virtual StepRunStatus
        run(const void *inputFromPrev, FlowError &error, bool &errorHandled,
            int &resumeStepId, int &successResumeStepId)
            = 0;
        virtual const void *outputPtr() const = 0;
      };

      std::vector<IRuntimeStep *> steps_;

      FlowChainImpl *clone() const {
        FlowChainImpl *next = new FlowChainImpl();
        next->successCallbacks_ = this->successCallbacks_;
        next->failureCallbacks_ = this->failureCallbacks_;
        next->finallyFn_ = this->finallyFn_;
        next->finallyUser_ = this->finallyUser_;
        next->loadingState_ = this->loadingState_;
        for (std::size_t i = 0; i < this->steps_.size(); ++i) {
          next->steps_.push_back(this->steps_[i]->clone());
        }
        return next;
      }

    private:
      int refs_;
    };

    template <typename AdapterT>
    class RuntimeStep : public FlowChainImpl::IRuntimeStep {
    public:
      typedef StepSpec<AdapterT> Spec;
      typedef typename Spec::In In;
      typedef typename Spec::Out Out;

      explicit RuntimeStep(const Spec &spec) : spec_(spec), out_() {
      }

      virtual FlowChainImpl::IRuntimeStep *clone() const {
        return new RuntimeStep<AdapterT>(this->spec_);
      }

      virtual int stepId() const {
        return this->spec_.id();
      }

      virtual StepRunStatus
      run(const void *inputFromPrev, FlowError &error, bool &errorHandled,
          int &resumeStepId, int &successResumeStepId) {
        errorHandled = false;
        resumeStepId = -1;
        successResumeStepId = -1;
        const In *in = this->spec_.inputPtr();
        if (in == 0) {
          in = static_cast<const In *>(inputFromPrev);
        }
        assert(in != 0);

        StepRunStatus status = this->spec_.adapter().run(*in, this->out_, error);
        if (status == FLOW_STEP_SUCCEEDED) {
          const std::vector<typename Spec::SuccessCallback> &callbacks
              = this->spec_.successCallbacks();
          for (std::size_t i = 0; i < callbacks.size(); ++i) {
            if (callbacks[i].fn != 0) {
              callbacks[i].fn(this->out_, callbacks[i].user);
            }
            if (successResumeStepId < 0 && callbacks[i].resumeStepId >= 0) {
              successResumeStepId = callbacks[i].resumeStepId;
            }
          }

          const std::vector<Out *> &states = this->spec_.successStates();
          for (std::size_t i = 0; i < states.size(); ++i) {
            if (states[i] != 0) {
              *states[i] = this->out_;
            }
          }

          if (this->spec_.finallyFn() != 0) {
            this->spec_.finallyFn()(this->spec_.finallyUser());
          }
          return FLOW_STEP_SUCCEEDED;
        }

        if (status == FLOW_STEP_PENDING) {
          return FLOW_STEP_PENDING;
        }

        const std::vector<typename Spec::FailureCallback> &failureCallbacks
            = this->spec_.failureCallbacks();
        for (std::size_t i = 0; i < failureCallbacks.size(); ++i) {
          const typename Spec::FailureCallback &cb = failureCallbacks[i];
          if (cb.matcher != 0 && cb.matcher(error, cb.user)) {
            const bool handled
                = (cb.handler != 0
                   && cb.handler(error, cb.user) == FLOW_ERROR_HANDLED);
            errorHandled = handled;
            if (handled && cb.resumeStepId >= 0) {
              resumeStepId = cb.resumeStepId;
            }
            break; // first-match-wins
          }
        }

        if (this->spec_.finallyFn() != 0) {
          this->spec_.finallyFn()(this->spec_.finallyUser());
        }
        return FLOW_STEP_FAILED;
      }

      virtual const void *outputPtr() const {
        return &this->out_;
      }

    private:
      Spec spec_;
      Out out_;
    };

    template <typename InT, typename OutT> class FlowChain {
    public:
      typedef InT In;
      typedef OutT Out;
      typedef bool (*ErrorMatcherFn)(const FlowError &, void *);
      typedef FlowHandleResult (*ErrorHandlerFn)(const FlowError &, void *);

      explicit FlowChain(FlowChainImpl *impl) : impl_(impl) {
      }

      FlowChain(const FlowChain &other) : impl_(other.impl_) {
        this->impl_->retain();
      }

      ~FlowChain() {
        this->impl_->release();
      }

      FlowChain &operator=(const FlowChain &other) {
        if (this != &other) {
          this->impl_->release();
          this->impl_ = other.impl_;
          this->impl_->retain();
        }
        return *this;
      }

      template <typename NextAdapterT>
      FlowChain<InT, typename NextAdapterT::Out>
      operator|(const StepSpec<NextAdapterT> &step) const {
        typedef typename NextAdapterT::In NextIn;
        typedef typename NextAdapterT::Out NextOut;
        typedef char LokaFlowStepTypeMismatch
            [(flow_detail::IsSame<OutT, NextIn>::value) ? 1 : -1];
        (void)sizeof(LokaFlowStepTypeMismatch);

        FlowChainImpl *nextImpl = this->impl_->clone();
        nextImpl->steps_.push_back(new RuntimeStep<NextAdapterT>(step));
        return FlowChain<InT, NextOut>(nextImpl);
      }

      FlowChain &onSuccess(void (*fn)(void *), void *user) {
        this->detachIfShared();
        FlowChainImpl::FlowSuccessCallback cb;
        cb.fn = fn;
        cb.user = user;
        cb.resumeStepId = -1;
        this->impl_->successCallbacks_.push_back(cb);
        return *this;
      }

      FlowChain &onSuccess(void (*fn)(void *), void *user, int resumeStepId) {
        this->detachIfShared();
        FlowChainImpl::FlowSuccessCallback cb;
        cb.fn = fn;
        cb.user = user;
        cb.resumeStepId = resumeStepId;
        this->impl_->successCallbacks_.push_back(cb);
        return *this;
      }

      FlowChain &
      onFailure(ErrorMatcherFn matcher, ErrorHandlerFn handler, void *user) {
        this->detachIfShared();
        FlowChainImpl::FlowFailureCallback cb;
        cb.matcher = matcher;
        cb.handler = handler;
        cb.user = user;
        cb.resumeStepId = -1;
        this->impl_->failureCallbacks_.push_back(cb);
        return *this;
      }

      FlowChain &onFailure(ErrorMatcherFn matcher, ErrorHandlerFn handler,
                           void *user, int resumeStepId) {
        this->detachIfShared();
        FlowChainImpl::FlowFailureCallback cb;
        cb.matcher = matcher;
        cb.handler = handler;
        cb.user = user;
        cb.resumeStepId = resumeStepId;
        this->impl_->failureCallbacks_.push_back(cb);
        return *this;
      }

      FlowChain &onFailure(ErrorHandlerFn handler, void *user) {
        return this->onFailure(&FlowChain::alwaysMatch, handler, user);
      }

      FlowChain &onFailure(ErrorHandlerFn handler, void *user, int resumeStepId) {
        return this->onFailure(&FlowChain::alwaysMatch, handler, user,
                               resumeStepId);
      }

      FlowChain &onFinally(void (*fn)(void *), void *user) {
        this->detachIfShared();
        this->impl_->finallyFn_ = fn;
        this->impl_->finallyUser_ = user;
        return *this;
      }

      FlowChain &trackLoading(bool *loadingState) {
        this->detachIfShared();
        this->impl_->loadingState_ = loadingState;
        return *this;
      }

      bool run() const {
        return this->runResult() == FLOW_RUN_SUCCEEDED;
      }

      FlowRunResult runResult() const {
        return this->runFromIndex(0);
      }

      bool resume(int stepId) const {
        return this->resumeResult(stepId) == FLOW_RUN_SUCCEEDED;
      }

      FlowRunResult resumeResult(int stepId) const {
        std::size_t start = 0;
        if (!this->findStepIndex(stepId, start)) {
          return FLOW_RUN_FAILED;
        }
        return this->runFromIndex(start);
      }

    private:
      FlowRunResult runFromIndex(std::size_t startIndex) const {
        if (startIndex >= this->impl_->steps_.size()) {
          return FLOW_RUN_FAILED;
        }

        const void *current = 0;
        FlowError error;
        if (this->impl_->loadingState_ != 0) {
          *this->impl_->loadingState_ = true;
        }

        for (std::size_t i = startIndex; i < this->impl_->steps_.size(); ++i) {
          bool stepHandled = false;
          int stepResumeStepId = -1;
          int stepSuccessResumeStepId = -1;
          StepRunStatus stepStatus
              = this->impl_->steps_[i]->run(current, error, stepHandled,
                                            stepResumeStepId,
                                            stepSuccessResumeStepId);
          if (stepStatus == FLOW_STEP_SUCCEEDED) {
            current = this->impl_->steps_[i]->outputPtr();
            int flowSuccessResumeStepId = -1;
            for (std::size_t k = 0; k < this->impl_->successCallbacks_.size();
                 ++k) {
              this->impl_->successCallbacks_[k].fn(
                  this->impl_->successCallbacks_[k].user);
              if (flowSuccessResumeStepId < 0
                  && this->impl_->successCallbacks_[k].resumeStepId >= 0) {
                flowSuccessResumeStepId
                    = this->impl_->successCallbacks_[k].resumeStepId;
              }
            }

            const int jumpStepId = stepSuccessResumeStepId >= 0
                                       ? stepSuccessResumeStepId
                                       : flowSuccessResumeStepId;
            if (jumpStepId >= 0) {
              std::size_t jumpIndex = 0;
              if (!this->findStepIndex(jumpStepId, jumpIndex)) {
                if (this->impl_->finallyFn_ != 0) {
                  this->impl_->finallyFn_(this->impl_->finallyUser_);
                }
                if (this->impl_->loadingState_ != 0) {
                  *this->impl_->loadingState_ = false;
                }
                return FLOW_RUN_FAILED;
              }
              i = (jumpIndex == 0) ? static_cast<std::size_t>(-1)
                                   : (jumpIndex - 1);
            }
            continue;
          }

          if (stepStatus == FLOW_STEP_PENDING) {
            return FLOW_RUN_PENDING;
          }

          bool flowHandled = false;
          int flowResumeStepId = -1;
          for (std::size_t k = 0; k < this->impl_->failureCallbacks_.size();
               ++k) {
            const FlowChainImpl::FlowFailureCallback &cb
                = this->impl_->failureCallbacks_[k];
            if (cb.matcher != 0 && cb.matcher(error, cb.user)) {
              const bool handled
                  = (cb.handler != 0
                     && cb.handler(error, cb.user) == FLOW_ERROR_HANDLED);
              flowHandled = handled;
              if (handled && cb.resumeStepId >= 0) {
                flowResumeStepId = cb.resumeStepId;
              }
              break; // first-match-wins
            }
          }

          const int jumpStepId
              = stepResumeStepId >= 0 ? stepResumeStepId : flowResumeStepId;
          if (jumpStepId >= 0) {
            std::size_t jumpIndex = 0;
            if (!this->findStepIndex(jumpStepId, jumpIndex)) {
              if (this->impl_->finallyFn_ != 0) {
                this->impl_->finallyFn_(this->impl_->finallyUser_);
              }
              if (this->impl_->loadingState_ != 0) {
                *this->impl_->loadingState_ = false;
              }
              return FLOW_RUN_FAILED;
            }
            i = (jumpIndex == 0) ? static_cast<std::size_t>(-1) : (jumpIndex - 1);
            continue;
          }

          if (this->impl_->finallyFn_ != 0) {
            this->impl_->finallyFn_(this->impl_->finallyUser_);
          }
          if (this->impl_->loadingState_ != 0) {
            *this->impl_->loadingState_ = false;
          }
          return stepHandled || flowHandled ? FLOW_RUN_SUCCEEDED : FLOW_RUN_FAILED;
        }

        if (this->impl_->finallyFn_ != 0) {
          this->impl_->finallyFn_(this->impl_->finallyUser_);
        }
        if (this->impl_->loadingState_ != 0) {
          *this->impl_->loadingState_ = false;
        }

        return FLOW_RUN_SUCCEEDED;
      }

      bool findStepIndex(int stepId, std::size_t &indexOut) const {
        for (std::size_t i = 0; i < this->impl_->steps_.size(); ++i) {
          if (this->impl_->steps_[i]->stepId() == stepId) {
            indexOut = i;
            return true;
          }
        }
        return false;
      }

    public:
      static bool alwaysMatch(const FlowError &, void *) {
        return true;
      }

      void detachIfShared() {
        FlowChainImpl *next = this->impl_->clone();
        this->impl_->release();
        this->impl_ = next;
      }

      FlowChainImpl *impl_;
    };

    class Flow {
    public:
      template <typename AdapterT>
      FlowChain<typename AdapterT::In, typename AdapterT::Out>
      operator|(const StepSpec<AdapterT> &step) const {
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
