#include "CardFlow.hpp"
#include "CardNodes.hpp"
#include "JsNativeClass.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include <cassert>
#include <cstdio>

namespace smirkycard
{
  JSClassID JsFlowDescription::classId_ = 0;
  JSClassID JsFlowDescription::skipClassId_ = 0;

  JsFlowDescription *JsFlowDescription::get(JSValueConst value)
  {
    return static_cast<JsFlowDescription *>(JS_GetOpaque(value, classId_));
  }
  bool JsFlowDescription::isSkip(JSValueConst value)
  {
    return JS_IsObject(value) && JS_GetClassID(value) == skipClassId_;
  }
  bool JsFlowDescription::install(JSContext *ctx)
  {
    JSRuntime *rt = JS_GetRuntime(ctx);
    if (!JsCardNode::installCapability(rt))
      return false;
    JSClassDef def;
    memset(&def, 0, sizeof(def));
    def.class_name = "SmirkyFlowDescription";
    def.finalizer = finalize;
    def.gc_mark = mark;
    if (!RegisterJsNativeClass(rt, classId_, def))
      return false;
    memset(&def, 0, sizeof(def));
    def.class_name = "SmirkyFlowSkip";
    if (!RegisterJsNativeClass(rt, skipClassId_, def))
      return false;
    JSValue builder = JS_NewCFunction(ctx, build, "Flow", 0);
    JSValue skip = JS_NewObjectClass(ctx, skipClassId_);
    bool ok = !JS_IsException(builder) && !JS_IsException(skip) && JS_FreezeObject(ctx, skip) >= 0;
    if (ok)
      ok = JS_DefinePropertyValueStr(ctx, builder, "SKIP", JS_DupValue(ctx, skip), 0) >= 0;
    JS_FreeValue(ctx, skip);
    if (ok)
      ok = JS_FreezeObject(ctx, builder) >= 0;
    JSValue global = JS_GetGlobalObject(ctx);
    if (ok)
      ok = JS_SetPropertyStr(ctx, global, "Flow", JS_DupValue(ctx, builder)) >= 0;
    JS_FreeValue(ctx, builder);
    JS_FreeValue(ctx, global);
    return ok;
  }

  /** Owns the immediate subscription; withdrawal is idempotent. BindingToken's
      callback may outlive the active subscription, but never the card record. */
  class CardFlow::Subscription
  {
  public:
    struct Initial
    {
      loka::core::StateBase &state;
      loka::core::StateBase::OnChangeFn callback;
      void *user;
      Initial(loka::core::StateBase &s, loka::core::StateBase::OnChangeFn cb, void *u)
          : state(s),
            callback(cb),
            user(u)
      {
      }
    };
    explicit Subscription(const Initial &i)
        : state_(i.state),
          callback_(i.callback),
          user_(i.user)
    {
      this->state_.bind(this->callback_, this->user_, false);
    }
    ~Subscription()
    {
      this->state_.unbind(this->callback_, this->user_);
    }

  private:
    loka::core::StateBase &state_;
    const loka::core::StateBase::OnChangeFn callback_;
    void *const user_;
    Subscription(const Subscription &);
    Subscription &operator=(const Subscription &);
  };

  CardFlow::CardFlow(const Initial &i)
      : next(0),
        card_(i.card),
        context_(i.context),
        description_(JS_DupValue(i.context, i.description)),
        handle_(JS_DupValue(i.context, i.handle)),
        seat_(i.seat),
        subscription_(0)
  {
  }
  CardFlow::~CardFlow()
  {
    assert(!this->subscription_ && "CardFlow watch must be withdrawn before reclamation");
    JS_FreeValue(this->context_, this->description_);
    JS_FreeValue(this->context_, this->handle_);
  }
  bool CardFlow::matches(JSValueConst handle) const
  {
    return JS_IsStrictEqual(this->context_, this->handle_, handle);
  }
  bool CardFlow::activate(loka::app::scene::BindingToken &token)
  {
    if (!this->seat_)
      return true;
    if (!this->seat_->isMaterialized())
      return false;
    token.watch(*this, this, &CardFlow::fire, false);
    return this->subscription_ != 0;
  }
  void CardFlow::bind(loka::core::StateBase::OnChangeFn callback, void *user, bool immediate)
  {
    (void)immediate;
    assert(!immediate && !this->subscription_);
    loka::core::StateBase *state = 0;
    switch (this->seat_->kind)
    {
    case JsSeatRecord::STRING:
      state = this->seat_->string.state();
      break;
    case JsSeatRecord::INTEGER:
      state = this->seat_->integer.state();
      break;
    case JsSeatRecord::BOOLEAN:
      state = this->seat_->boolean.state();
      break;
    }
    if (!state)
      return;
    this->subscription_ = loka::core::LokaNew<Subscription>(loka::core::LokaAllocationSite("CardFlow", "Subscription"),
                                                            Subscription::Initial(*state, callback, user));
  }
  void CardFlow::unbind(loka::core::StateBase::OnChangeFn, void *)
  {
    this->withdraw();
  }
  void CardFlow::withdraw()
  {
    loka::core::LokaDelete(this->subscription_, loka::core::LokaAllocationSite("CardFlow", "Subscription"));
    this->subscription_ = 0;
  }
  bool CardFlow::live() const
  {
    return this->card_.phase_ == JsCardNode::Live && !this->card_.failed_ && this->card_.scene();
  }
  void CardFlow::fire()
  {
    this->run(JS_UNDEFINED, true);
  }

  CardFlow::CallResult CardFlow::invoke(JSValueConst fn, JSValueConst input, JSValue &output)
  {
    if (!this->live())
      return Canceled;
    output = this->card_.props.runtime->rawCall(*this->card_.engine_, fn, 1, &input);
    // This gate must precede captureException: formatting can execute arbitrary JS.
    if (!this->live())
    {
      if (JS_IsException(output))
        JS_FreeValue(this->context_, JS_GetException(this->context_));
      JS_FreeValue(this->context_, output);
      output = JS_UNDEFINED;
      return Canceled;
    }
    if (!JS_IsException(output))
      return Returned;
    loka::core::String error;
    this->card_.props.runtime->captureException(*this->card_.engine_, error);
    output = JS_UNDEFINED;
    // A thrown formatter can leave a secondary exception. Discard it without JS.
    if (JS_HasException(this->context_))
      JS_FreeValue(this->context_, JS_GetException(this->context_));
    if (!this->live())
      return Canceled;
    this->card_.error_.set(error);
    const loka::core::StringBuffer bytes = error.bufferWithEncoding(loka::core::StringEncodingUtf8);
    output = JS_NewStringLen(this->context_, static_cast<const char *>(bytes.data()), bytes.length());
    return Threw;
  }

  CardFlow::Admission::Execution::Execution(Admission &door, CardFlow &flow, bool watching)
      : door_(0)
  {
    if (door.active_)
    {
      if (watching && door.active_ != &flow)
      {
        std::fputs("CardFlow: nested watch firing dropped (stage 2a)\n", stderr);
        assert(false && "CardFlow: nested watch firing dropped (stage 2a)");
      }
      return;
    }
    this->door_ = &door;
    this->door_->active_ = &flow;
  }
  CardFlow::Admission::Execution::~Execution()
  {
    if (this->door_)
      this->door_->active_ = 0;
  }

  bool CardFlow::run(JSValueConst input, bool watching)
  {
    if (!this->live())
      return false;
    Admission::Execution execution(this->card_.flowAdmission_, *this, watching);
    if (!execution.accepted())
      return false;
    loka::core::StateTrackerGuard transaction(this->card_.tracker());
    ScriptRuntime::InterruptWindow interrupt(*this->card_.props.runtime, *this->card_.engine_);
    JsFlowDescription *description = JsFlowDescription::get(this->description_);
    const JsFlowDescription::StepRecord *step = description->executionSteps();
    JSValue value =
        watching ? this->card_.seatGet(this->context_, this->seat_->value) : JS_DupValue(this->context_, input);
    if (!watching && this->seat_)
      step = step->next;
    CallResult result = Returned;
    for (; step; step = step->next)
    {
      JSValue output = JS_UNDEFINED;
      result = this->invoke(step->fn, value, output);
      JS_FreeValue(this->context_, value);
      value = output;
      if (result != Returned)
        break;
      if (watching && step == description->executionSteps() && JsFlowDescription::isSkip(value))
      {
        JS_FreeValue(this->context_, value);
        return true;
      }
    }
    if (result != Canceled && this->live())
    {
      JSValue callback = description->callback(result == Returned);
      if (JS_IsFunction(this->context_, callback))
      {
        JSValue output = JS_UNDEFINED;
        // Terminal exceptions are diagnostic only: never notify a second time.
        this->invoke(callback, value, output);
        JS_FreeValue(this->context_, output);
      }
    }
    JS_FreeValue(this->context_, value);
    return true;
  }
} // namespace smirkycard
