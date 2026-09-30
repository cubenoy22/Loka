#include "CardScenario.hpp"
#ifdef TEST_BUILD
#include "CardNodes.hpp"
#include "JsNativeClass.hpp"
#include "app/nodes/AttributedText.hpp"
#include <new>
#include <stdint.h>
#include <cfloat>

namespace smirkycard
{
  using namespace loka::dsl;
  using namespace loka::dsl::testing;
  namespace
  {
    JSClassID builderClass = 0;
    enum BuilderMethod
    {
      Step,
      Named,
      Success,
      Failure
    };
    enum TestMethod
    {
      Run,
      Click,
      Enabled,
      Text,
      Log,
      Random
    };
    std::string utf8(const loka::core::String &value)
    {
      const loka::core::StringBuffer b = value.bufferWithEncoding(loka::core::StringEncodingUtf8);
      return std::string(static_cast<const char *>(b.data()), b.length());
    }
    JSValue jsText(JSContext *ctx, const loka::core::String &value)
    {
      const std::string text = utf8(value);
      return JS_NewStringLen(ctx, text.data(), text.size());
    }
  } // namespace

  /** QuickJS-owned mutable description; acceptance closes its only mutation door.
      The GC marker accounts for functions that close over their own builder. */
  class JsFlowDescription
  {
  public:
    struct StepRecord
    {
      StepRecord(JSContext *ctx, JSValueConst value)
          : fn(JS_DupValue(ctx, value)),
            next(0)
      {
      }
      JSValue fn;
      std::string name;
      StepRecord *next;
    };
    explicit JsFlowDescription(JSRuntime *runtime)
        : runtime_(runtime),
          head_(0),
          tail_(0),
          success_(JS_UNDEFINED),
          failure_(JS_UNDEFINED),
          consumed_(false)
    {
    }
    ~JsFlowDescription()
    {
      while (this->head_)
      {
        StepRecord *step = this->head_;
        this->head_ = step->next;
        JS_FreeValueRT(this->runtime_, step->fn);
        delete step;
      }
      JS_FreeValueRT(this->runtime_, this->success_);
      JS_FreeValueRT(this->runtime_, this->failure_);
    }
    const StepRecord *steps() const
    {
      return this->consumed_ ? 0 : this->head_;
    }
    JSValue callback(bool success) const
    {
      return success ? this->success_ : this->failure_;
    }
    void consume()
    {
      this->consumed_ = true;
    }
    static void finalize(JSRuntime *, JSValue value)
    {
      delete static_cast<JsFlowDescription *>(JS_GetOpaque(value, builderClass));
    }
    static void mark(JSRuntime *rt, JSValueConst value, JS_MarkFunc *fn)
    {
      JsFlowDescription *self = static_cast<JsFlowDescription *>(JS_GetOpaque(value, builderClass));
      if (!self)
        return;
      for (StepRecord *s = self->head_; s; s = s->next)
        JS_MarkValue(rt, s->fn, fn);
      JS_MarkValue(rt, self->success_, fn);
      JS_MarkValue(rt, self->failure_, fn);
    }
    static JSValue build(JSContext *ctx, JSValueConst, int, JSValueConst *)
    {
      JSValue object = JS_NewObjectClass(ctx, builderClass);
      if (JS_IsException(object))
        return object;
      JsFlowDescription *self = new (std::nothrow) JsFlowDescription(JS_GetRuntime(ctx));
      if (!self)
      {
        JS_FreeValue(ctx, object);
        return JS_ThrowOutOfMemory(ctx);
      }
      JS_SetOpaque(object, self);
      const char *names[] = {"step", "named", "onSuccess", "onFailure"};
      for (int i = 0; i < 4; ++i)
        if (JS_SetPropertyStr(
                ctx, object, names[i], JS_NewCFunctionMagic(ctx, method, names[i], 1, JS_CFUNC_generic_magic, i))
            < 0)
        {
          JS_FreeValue(ctx, object);
          return JS_EXCEPTION;
        }
      // The native mutation door changes the description, never object properties.
      if (JS_FreezeObject(ctx, object) < 0)
      {
        JS_FreeValue(ctx, object);
        return JS_EXCEPTION;
      }
      return object;
    }
    static JSValue method(JSContext *ctx, JSValueConst receiver, int argc, JSValueConst *argv, int op)
    {
      JsFlowDescription *self = static_cast<JsFlowDescription *>(JS_GetOpaque(receiver, builderClass));
      if (!self || self->consumed_)
        return JS_ThrowTypeError(ctx, "Flow description is consumed or invalid");
      if (argc != 1 || (op == Named ? !JS_IsString(argv[0]) : !JS_IsFunction(ctx, argv[0])))
        return JS_ThrowTypeError(ctx, "Flow method requires a function (named requires a string)");
      if (op == Step)
      {
        StepRecord *s = new (std::nothrow) StepRecord(ctx, argv[0]);
        if (!s)
          return JS_ThrowOutOfMemory(ctx);
        if (self->tail_)
          self->tail_->next = s;
        else
          self->head_ = s;
        self->tail_ = s;
      }
      else if (op == Named)
      {
        if (!self->tail_)
          return JS_ThrowTypeError(ctx, "named requires a preceding step");
        size_t n = 0;
        const char *name = JS_ToCStringLen(ctx, &n, argv[0]);
        if (!name)
          return JS_EXCEPTION;
        self->tail_->name.assign(name, n);
        JS_FreeCString(ctx, name);
      }
      else
      {
        JSValue &slot = op == Success ? self->success_ : self->failure_;
        JS_FreeValue(ctx, slot);
        slot = JS_DupValue(ctx, argv[0]);
      }
      return JS_DupValue(ctx, receiver);
    }

  private:
    JSRuntime *runtime_;
    StepRecord *head_, *tail_;
    JSValue success_, failure_;
    bool consumed_;
  };

  bool CardScenario::installBuilder(JSContext *ctx)
  {
    JSRuntime *rt = JS_GetRuntime(ctx);
    // Reserve the capability first even though the card factory runs later.
    // Every engine uses this same installation order, including runner-off cards.
    if (!JsCardNode::installScenarioCapability(rt))
      return false;
    JSClassDef def;
    memset(&def, 0, sizeof(def));
    def.class_name = "SmirkyFlowDescription";
    def.finalizer = JsFlowDescription::finalize;
    def.gc_mark = JsFlowDescription::mark;
    if (!RegisterJsNativeClass(rt, builderClass, def))
      return false;
    JSValue global = JS_GetGlobalObject(ctx);
    const bool ok =
        JS_SetPropertyStr(ctx, global, "Flow", JS_NewCFunction(ctx, JsFlowDescription::build, "Flow", 0)) >= 0;
    JS_FreeValue(ctx, global);
    return ok;
  }

  CardScenario::CardScenario(JsCardNode &card, JSValueConst context)
      : card_(card),
        runtime_(*card.props.runtime),
        engine_(*card.engine_),
        sink_(*this->runtime_.runner_->sink),
        clock_(*this->runtime_.runner_->clock),
        input_(0),
        context_(JS_DupValue(this->engine_.context(), context)),
        scenario_(this->engine_.takeScenario()),
        description_(JS_UNDEFINED),
        previous_(JS_UNDEFINED),
        chain_(0),
        phase_(JS_IsUndefined(this->scenario_) ? Done : Waiting),
        outcome_(FLOW_RUN_PENDING),
        randomState_(this->runtime_.runner_->seed & 0xffffffffUL),
        terminal_(&this->sink_)
  {
  }

  CardScenario::~CardScenario()
  {
    // No callbacks, audit writes, or cancellation delivery from reclamation.
    delete this->chain_;
    JSContext *ctx = this->engine_.context();
    JS_FreeValue(ctx, this->previous_);
    JS_FreeValue(ctx, this->description_);
    JS_FreeValue(ctx, this->scenario_);
    JS_FreeValue(ctx, this->context_);
  }
  bool CardScenario::installContext(JSContext *ctx, JSValueConst context, JSValue capability, JSCFunctionData *method)
  {
    if (JS_SetPropertyStr(ctx, context, "run", JS_NewCFunctionData(ctx, method, 1, Run, 1, &capability)) < 0)
      return false;
    JSValue test = JS_NewObject(ctx);
    const char *names[] = {"click", "enabled", "text", "log", "random"};
    bool ok = !JS_IsException(test);
    for (int i = 0; ok && i < 5; ++i)
      ok = JS_SetPropertyStr(
               ctx, test, names[i], JS_NewCFunctionData(ctx, method, i == 4 ? 0 : 1, Click + i, 1, &capability))
           >= 0;
    ok = ok && JS_FreezeObject(ctx, test) >= 0;
    if (!ok)
    {
      JS_FreeValue(ctx, test);
      return false;
    }
    return JS_SetPropertyStr(ctx, context, "test", test) >= 0;
  }
  void CardScenario::attach(CardScene *scene)
  {
    if (this->card_.phase_ != JsCardNode::Live)
      return;
    this->input_ = scene;
    assert(!scene->runnerCard_ || scene->runnerCard_ == &this->card_);
    scene->runnerCard_ = &this->card_;
  }
  void CardScene::tickScenario()
  {
    if (this->runnerCard_ && this->runnerCard_->scenario_)
      this->runnerCard_->scenario_->tick();
  }
  bool CardScenario::canAdvance() const
  {
    return this->card_.phase_ == JsCardNode::Live && this->outcome_ != FLOW_RUN_CANCELED;
  }
  void CardScenario::cancel()
  {
    if (this->input_)
    {
      static_cast<CardScene *>(this->input_)->runnerCard_ = 0;
      this->input_ = 0;
    }
    if (this->phase_ != Done)
      this->outcome_ = FLOW_RUN_CANCELED;
  }
  void CardScenario::finishDetach()
  {
    if (this->phase_ == Waiting)
      this->phase_ = Done;
    else if (this->phase_ == Idle)
      this->complete(FLOW_RUN_CANCELED);
  }
  bool CardScenario::invoke(JSValueConst fn, int argc, JSValueConst *argv, JSValue &result, loka::core::String &error)
  {
    loka::core::StateTrackerGuard guard(this->card_.tracker());
    return this->runtime_.call(this->engine_, fn, argc, argv, result, error);
  }
  StepRunStatus CardScenario::JsStepAdapter::run(const In &in, Out &out, FlowError &error) const
  {
    out = in;
    if (!this->owner_->canAdvance())
      return FLOW_STEP_PENDING;
    JSValue result = JS_UNDEFINED;
    loka::core::String message;
    this->owner_->operationError_ = FlowError();
    const bool ok = this->owner_->invoke(this->fn_, 1, &this->owner_->previous_, result, message);
    if (ok)
    {
      JS_FreeValue(this->owner_->engine_.context(), this->owner_->previous_);
      this->owner_->previous_ = result;
      return FLOW_STEP_SUCCEEDED;
    }
    JS_FreeValue(this->owner_->engine_.context(), result);
    this->message_ = utf8(message);
    error = this->owner_->operationError_;
    if (!error.kind)
    {
      error.kind = FLOW_ERROR_KIND_FLOW;
      error.code = FLOW_ERROR_CODE_ASSERT_PREDICATE_FAILED;
    }
    return FLOW_STEP_FAILED;
  }
  StepRunStatus CardScenario::GatedSettle::run(const In &in, Out &out, FlowError &error) const
  {
    out = in;
    return this->owner_->canAdvance() ? Settle().run(in, out, error) : FLOW_STEP_PENDING;
  }
  JSValue CardScenario::accept(JSContext *ctx, JSValueConst value)
  {
    if (this->phase_ != Invoking || this->chain_ || !this->canAdvance())
      return JS_ThrowTypeError(ctx, "c.run is only accepted once synchronously inside this card's scenario invocation");
    JsFlowDescription *description = static_cast<JsFlowDescription *>(JS_GetOpaque(value, builderClass));
    if (!description || !description->steps())
      return JS_ThrowTypeError(ctx, "c.run requires an unconsumed Flow with at least one step");
    const JsFlowDescription::StepRecord *s = description->steps();
    ScenarioFlowChain<ScenePtr, ScenePtr> flow = ScenarioFlow(this->clock_, &this->input_).auditTo(&this->sink_)
                                                 | Then(RunOnce(JsStepAdapter(*this, s->fn))).named(s->name.c_str());
    flow = flow | Then(GatedSettle(*this));
    for (s = s->next; s; s = s->next)
    {
      flow = flow | Then(RunOnce(JsStepAdapter(*this, s->fn))).named(s->name.c_str());
      flow = flow | Then(GatedSettle(*this));
    }
    this->chain_ = new (std::nothrow) Chain(flow.flow());
    if (!this->chain_)
      return JS_ThrowOutOfMemory(ctx);
    this->description_ = JS_DupValue(ctx, value);
    description->consume();
    return JS_UNDEFINED;
  }
  void CardScenario::tick()
  {
    if (this->phase_ != Waiting && this->phase_ != Idle)
      return;
    if (!this->canAdvance())
    {
      if (this->outcome_ == FLOW_RUN_CANCELED)
        this->complete(FLOW_RUN_CANCELED);
      return;
    }
    if (this->outcome_ != FLOW_RUN_PENDING)
    {
      this->complete(this->outcome_);
      return;
    }
    if (this->phase_ == Waiting)
    {
      this->phase_ = Invoking;
      JSValue result = JS_UNDEFINED;
      loka::core::String error;
      const bool ok = this->invoke(this->scenario_, 1, &this->context_, result, error);
      JS_FreeValue(this->engine_.context(), result);
      this->phase_ = Idle;
      if (!ok || !this->chain_)
      {
        const std::string message = ok ? "scenario did not call c.run" : utf8(error);
        this->sink_.recordLog(message);
        this->outcome_ = this->outcome_ == FLOW_RUN_CANCELED ? FLOW_RUN_CANCELED : FLOW_RUN_FAILED;
        if (this->canAdvance() || this->outcome_ == FLOW_RUN_CANCELED)
          this->complete(this->outcome_);
        return;
      }
      if (!this->canAdvance())
      {
        if (this->outcome_ == FLOW_RUN_CANCELED)
          this->complete(FLOW_RUN_CANCELED);
        return;
      }
    }
    this->phase_ = Driving;
    const FlowRunResult result = this->chain_->runResult();
    this->phase_ = Idle;
    if (this->outcome_ == FLOW_RUN_CANCELED)
      this->complete(FLOW_RUN_CANCELED);
    else if (result != FLOW_RUN_PENDING)
    {
      this->outcome_ = result;
      if (this->canAdvance())
        this->complete(result);
    }
  }
  void CardScenario::complete(FlowRunResult result)
  {
    if (this->phase_ == Done)
      return;
    if (this->outcome_ == FLOW_RUN_CANCELED)
      result = FLOW_RUN_CANCELED;
    this->phase_ = Completing;
    if (result != FLOW_RUN_CANCELED && this->canAdvance() && !JS_IsUndefined(this->description_))
    {
      JsFlowDescription *desc = static_cast<JsFlowDescription *>(JS_GetOpaque(this->description_, builderClass));
      JSValue fn = desc->callback(result == FLOW_RUN_SUCCEEDED);
      if (JS_IsFunction(this->engine_.context(), fn))
      {
        JSValue returned = JS_UNDEFINED;
        loka::core::String error;
        if (!this->invoke(fn, 0, 0, returned, error))
        {
          this->sink_.recordLog(utf8(error));
          result = FLOW_RUN_FAILED;
        }
        JS_FreeValue(this->engine_.context(), returned);
      }
    }
    if (this->outcome_ == FLOW_RUN_CANCELED)
      result = FLOW_RUN_CANCELED;
    this->outcome_ = result;
    this->phase_ = Done;
    this->terminal_.emit(result == FLOW_RUN_CANCELED    ? SCENARIO_AUDIT_CANCELED
                         : result == FLOW_RUN_SUCCEEDED ? SCENARIO_AUDIT_SUCCEEDED
                                                        : SCENARIO_AUDIT_FAILED);
  }
  double CardScenario::random()
  {
    this->randomState_ = (this->randomState_ * 1664525UL + 1013904223UL) & 0xffffffffUL;
    // Normalize the integer into IEEE binary64's significand. No floating-point
    // arithmetic or integer-to-double conversion (Classic software-float safe).
    typedef char
        RequiresBinary64[(sizeof(double) == sizeof(uint64_t) && DBL_MANT_DIG == 53 && DBL_MAX_EXP == 1024) ? 1 : -1];
    (void)sizeof(RequiresBinary64);
    uint64_t bits = 0;
    if (this->randomState_)
    {
      unsigned long normalized = this->randomState_;
      unsigned int high = 31;
      while (!(normalized & 0x80000000UL))
      {
        normalized <<= 1;
        --high;
      }
      bits = (static_cast<uint64_t>(991 + high) << 52) | (static_cast<uint64_t>(normalized & 0x7fffffffUL) << 21);
    }
    double result;
    memcpy(&result, &bits, sizeof(result));
    return result;
  }
  JSValue CardScenario::operation(JSContext *ctx, int argc, JSValueConst *argv, int op)
  {
    if (op == Run)
      return argc == 1 ? this->accept(ctx, argv[0]) : JS_ThrowTypeError(ctx, "c.run requires one Flow");
    if (op == Random)
      return argc == 0 ? JS_NewFloat64(ctx, this->random()) : JS_ThrowTypeError(ctx, "random takes no arguments");
    if (argc != 1 || !JS_IsString(argv[0]))
      return JS_ThrowTypeError(ctx, "test operation requires a string");
    size_t length = 0;
    const char *bytes = JS_ToCStringLen(ctx, &length, argv[0]);
    if (!bytes)
      return JS_EXCEPTION;
    const std::string text(bytes, length);
    JS_FreeCString(ctx, bytes);
    if (op == Log)
      return this->sink_.recordLog(text) ? JS_UNDEFINED : JS_ThrowTypeError(ctx, "scenario audit log refused");
    if (!this->canAdvance() || !this->input_)
      return JS_ThrowTypeError(ctx, "test scene operation requires a Live mounted card");
    loka::app::scene::Node *node = 0;
    FlowError error;
    if (LookupNodeById(this->input_, text, node, error) != FLOW_STEP_SUCCEEDED)
    {
      this->operationError_ = error;
      return JS_ThrowTypeError(ctx, "test id '%s' is missing or duplicated", text.c_str());
    }
    if (op == Click)
    {
      ScenePtr out = 0;
      StepRunStatus status;
      if (node->asButtonNode())
        status = ClickButton(text.c_str(), CLICK_DISABLED_FAILS).run(this->input_, out, error);
      else if (node->asCellNode())
        status = ClickCell(text.c_str()).run(this->input_, out, error);
      else
        return JS_ThrowTypeError(ctx, "click requires a Button or Cell");
      if (status == FLOW_STEP_SUCCEEDED)
        return JS_UNDEFINED;
      this->operationError_ = error;
      if (error.code == FLOW_ERROR_SCENE_TEST_BUTTON_DISABLED)
        return JS_ThrowTypeError(ctx, "button '%s' is disabled", text.c_str());
      return JS_ThrowTypeError(ctx, "click '%s' failed (code %d)", text.c_str(), error.code);
    }
    if (op == Enabled)
    {
      if (node->asButtonNode())
        return JS_NewBool(ctx, SceneClickTraits<loka::app::ButtonNode>::enabled(node->asButtonNode()));
      if (node->asCellNode())
        return JS_TRUE;
      return JS_ThrowTypeError(ctx, "enabled requires a Button or Cell");
    }
    if (node->asTextNode())
      return jsText(ctx, node->asTextNode()->props.text_->get());
    if (node->asButtonNode())
      return jsText(ctx, node->asButtonNode()->props.text_->get());
    if (node->asEditTextNode())
      return jsText(ctx, node->asEditTextNode()->props.text_.state()->get());
    if (node->asAttributedTextNode())
    {
      const loka::app::AttributedString &value = node->asAttributedTextNode()->props.text_->get();
      loka::core::String flat;
      for (size_t i = 0; i < value.segmentCount(); ++i)
        flat = flat + value.segment(i).text;
      return jsText(ctx, flat);
    }
    return JS_ThrowTypeError(ctx, "text requires Text, Markup, Button or EditText");
  }
} // namespace smirkycard
#endif
