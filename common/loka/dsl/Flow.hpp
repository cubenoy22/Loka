#ifndef LOKA_DSL_FLOW_HPP
#define LOKA_DSL_FLOW_HPP

#include <cassert>
#include <vector>

namespace loka
{
  namespace dsl
  {
    struct FlowError
    {
      int kind;
      int code;
      FlowError() : kind(0), code(0) {}
    };

    enum FlowHandleResult
    {
      FLOW_ERROR_UNHANDLED = 0,
      FLOW_ERROR_HANDLED = 1
    };

    namespace flow_detail
    {
      template <typename A, typename B>
      struct IsSame
      {
        enum
        {
          value = 0
        };
      };

      template <typename A>
      struct IsSame<A, A>
      {
        enum
        {
          value = 1
        };
      };
    } // namespace flow_detail

    template <typename AdapterT>
    class StepSpec
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
      };

      struct FailureCallback
      {
        ErrorMatcherFn matcher;
        ErrorHandlerFn handler;
        void *user;
      };

      explicit StepSpec(int id, const AdapterT &adapter)
          : id_(id),
            adapter_(adapter),
            input_(0),
            finallyFn_(0),
            finallyUser_(0)
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
        this->successCallbacks_.push_back(cb);
        return *this;
      }

      StepSpec &onSuccess(Out *targetState)
      {
        this->successStates_.push_back(targetState);
        return *this;
      }

      StepSpec &onFailure(ErrorMatcherFn matcher, ErrorHandlerFn handler, void *user)
      {
        FailureCallback cb;
        cb.matcher = matcher;
        cb.handler = handler;
        cb.user = user;
        this->failureCallbacks_.push_back(cb);
        return *this;
      }

      StepSpec &onFailure(ErrorHandlerFn handler, void *user)
      {
        return this->onFailure(&StepSpec::alwaysMatch, handler, user);
      }

      StepSpec &onFinally(FinallyFn fn, void *user)
      {
        this->finallyFn_ = fn;
        this->finallyUser_ = user;
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

      FinallyFn finallyFn() const
      {
        return this->finallyFn_;
      }

      void *finallyUser() const
      {
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
    StepSpec<AdapterT> Step(int id, const AdapterT &adapter)
    {
      return StepSpec<AdapterT>(id, adapter);
    }

    class FlowChainImpl
    {
    public:
      FlowChainImpl() : finallyFn_(0), finallyUser_(0), refs_(1) {}

      ~FlowChainImpl()
      {
        for (std::size_t i = 0; i < this->steps_.size(); ++i)
        {
          delete this->steps_[i];
        }
      }

      void retain()
      {
        ++this->refs_;
      }

      void release()
      {
        --this->refs_;
        if (this->refs_ == 0)
        {
          delete this;
        }
      }

      struct FlowSuccessCallback
      {
        void (*fn)(void *);
        void *user;
      };

      struct FlowFailureCallback
      {
        bool (*matcher)(const FlowError &, void *);
        FlowHandleResult (*handler)(const FlowError &, void *);
        void *user;
      };

      std::vector<FlowSuccessCallback> successCallbacks_;
      std::vector<FlowFailureCallback> failureCallbacks_;
      void (*finallyFn_)(void *);
      void *finallyUser_;

      class IRuntimeStep
      {
      public:
        virtual ~IRuntimeStep() {}
        virtual IRuntimeStep *clone() const = 0;
        virtual bool run(const void *inputFromPrev, FlowError &error, bool &errorHandled) = 0;
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
        for (std::size_t i = 0; i < this->steps_.size(); ++i)
        {
          next->steps_.push_back(this->steps_[i]->clone());
        }
        return next;
      }

    private:
      int refs_;
    };

    template <typename AdapterT>
    class RuntimeStep : public FlowChainImpl::IRuntimeStep
    {
    public:
      typedef StepSpec<AdapterT> Spec;
      typedef typename Spec::In In;
      typedef typename Spec::Out Out;

      explicit RuntimeStep(const Spec &spec) : spec_(spec), out_()
      {
      }

      virtual FlowChainImpl::IRuntimeStep *clone() const
      {
        return new RuntimeStep<AdapterT>(this->spec_);
      }

      virtual bool run(const void *inputFromPrev, FlowError &error, bool &errorHandled)
      {
        errorHandled = false;
        const In *in = this->spec_.inputPtr();
        if (in == 0)
        {
          in = static_cast<const In *>(inputFromPrev);
        }
        assert(in != 0);

        if (this->spec_.adapter().run(*in, this->out_, error))
        {
          const std::vector<typename Spec::SuccessCallback> &callbacks = this->spec_.successCallbacks();
          for (std::size_t i = 0; i < callbacks.size(); ++i)
          {
            callbacks[i].fn(this->out_, callbacks[i].user);
          }

          const std::vector<Out *> &states = this->spec_.successStates();
          for (std::size_t i = 0; i < states.size(); ++i)
          {
            if (states[i] != 0)
            {
              *states[i] = this->out_;
            }
          }

          if (this->spec_.finallyFn() != 0)
          {
            this->spec_.finallyFn()(this->spec_.finallyUser());
          }
          return true;
        }

        const std::vector<typename Spec::FailureCallback> &failureCallbacks = this->spec_.failureCallbacks();
        for (std::size_t i = 0; i < failureCallbacks.size(); ++i)
        {
          const typename Spec::FailureCallback &cb = failureCallbacks[i];
          if (cb.matcher != 0 && cb.matcher(error, cb.user))
          {
            errorHandled = (cb.handler != 0 && cb.handler(error, cb.user) == FLOW_ERROR_HANDLED);
            break; // first-match-wins
          }
        }

        if (this->spec_.finallyFn() != 0)
        {
          this->spec_.finallyFn()(this->spec_.finallyUser());
        }
        return false;
      }

      virtual const void *outputPtr() const
      {
        return &this->out_;
      }

    private:
      Spec spec_;
      Out out_;
    };

    template <typename InT, typename OutT>
    class FlowChain
    {
    public:
      typedef InT In;
      typedef OutT Out;
      typedef bool (*ErrorMatcherFn)(const FlowError &, void *);
      typedef FlowHandleResult (*ErrorHandlerFn)(const FlowError &, void *);

      explicit FlowChain(FlowChainImpl *impl) : impl_(impl)
      {
      }

      FlowChain(const FlowChain &other) : impl_(other.impl_)
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
        this->impl_->failureCallbacks_.push_back(cb);
        return *this;
      }

      FlowChain &onFailure(ErrorHandlerFn handler, void *user)
      {
        return this->onFailure(&FlowChain::alwaysMatch, handler, user);
      }

      FlowChain &onFinally(void (*fn)(void *), void *user)
      {
        this->detachIfShared();
        this->impl_->finallyFn_ = fn;
        this->impl_->finallyUser_ = user;
        return *this;
      }

      bool run() const
      {
        const void *current = 0;
        FlowError error;
        bool finishedOk = true;

        for (std::size_t i = 0; i < this->impl_->steps_.size(); ++i)
        {
          bool stepHandled = false;
          if (this->impl_->steps_[i]->run(current, error, stepHandled))
          {
            current = this->impl_->steps_[i]->outputPtr();
            for (std::size_t k = 0; k < this->impl_->successCallbacks_.size(); ++k)
            {
              this->impl_->successCallbacks_[k].fn(this->impl_->successCallbacks_[k].user);
            }
            continue;
          }

          bool flowHandled = false;
          for (std::size_t k = 0; k < this->impl_->failureCallbacks_.size(); ++k)
          {
            const FlowChainImpl::FlowFailureCallback &cb = this->impl_->failureCallbacks_[k];
            if (cb.matcher != 0 && cb.matcher(error, cb.user))
            {
              flowHandled = (cb.handler != 0 && cb.handler(error, cb.user) == FLOW_ERROR_HANDLED);
              break; // first-match-wins
            }
          }

          finishedOk = stepHandled || flowHandled;
          break;
        }

        if (this->impl_->finallyFn_ != 0)
        {
          this->impl_->finallyFn_(this->impl_->finallyUser_);
        }

        return finishedOk;
      }

      static bool alwaysMatch(const FlowError &, void *)
      {
        return true;
      }

    private:
      void detachIfShared()
      {
        FlowChainImpl *next = this->impl_->clone();
        this->impl_->release();
        this->impl_ = next;
      }

      FlowChainImpl *impl_;
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
