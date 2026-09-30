#ifndef SMIRKYCARD_CARD_FLOW_DESCRIPTION_HPP
#define SMIRKYCARD_CARD_FLOW_DESCRIPTION_HPP
#include "ScriptRuntime.hpp"
#include <new>
namespace smirkycard
{
  /** QuickJS-owned mutable description; acceptance closes its only mutation door.
      The GC marker accounts for functions that close over their own builder. */
  class JsFlowDescription
  {
  public:
    static bool install(JSContext *ctx);
    static JsFlowDescription *get(JSValueConst value);
    static bool isSkip(JSValueConst value);
    bool hasWatch() const
    {
      return !JS_IsUninitialized(this->watched_);
    }
    JSValue watched() const
    {
      return this->watched_;
    }
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
          watched_(JS_UNINITIALIZED),
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
      JS_FreeValueRT(this->runtime_, this->watched_);
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
    const StepRecord *executionSteps() const
    {
      return this->head_;
    }
    void consume()
    {
      this->consumed_ = true;
    }
    static void finalize(JSRuntime *, JSValue value)
    {
      delete static_cast<JsFlowDescription *>(JS_GetOpaque(value, classId_));
    }
    static void mark(JSRuntime *rt, JSValueConst value, JS_MarkFunc *fn)
    {
      JsFlowDescription *self = static_cast<JsFlowDescription *>(JS_GetOpaque(value, classId_));
      if (!self)
        return;
      for (StepRecord *s = self->head_; s; s = s->next)
        JS_MarkValue(rt, s->fn, fn);
      JS_MarkValue(rt, self->watched_, fn);
      JS_MarkValue(rt, self->success_, fn);
      JS_MarkValue(rt, self->failure_, fn);
    }
    static JSValue build(JSContext *ctx, JSValueConst, int, JSValueConst *)
    {
      JSValue object = JS_NewObjectClass(ctx, classId_);
      if (JS_IsException(object))
        return object;
      JsFlowDescription *self = new (std::nothrow) JsFlowDescription(JS_GetRuntime(ctx));
      if (!self)
      {
        JS_FreeValue(ctx, object);
        return JS_ThrowOutOfMemory(ctx);
      }
      JS_SetOpaque(object, self);
      const char *names[] = {"step", "named", "onSuccess", "onFailure", "watch"};
      for (int i = 0; i < 5; ++i)
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
      JsFlowDescription *self = static_cast<JsFlowDescription *>(JS_GetOpaque(receiver, classId_));
      if (!self || self->consumed_)
        return JS_ThrowTypeError(ctx, "Flow description is consumed or invalid");
      if (op == Watch)
      {
        if (argc != 2 || !JS_IsFunction(ctx, argv[1]) || self->head_)
          return JS_ThrowTypeError(ctx, "watch(seat, adapter) must be the first step");
      }
      else if (argc != 1 || (op == Named ? !JS_IsString(argv[0]) : !JS_IsFunction(ctx, argv[0])))
        return JS_ThrowTypeError(ctx, "Flow method requires a function (named requires a string)");
      if (op == Step || op == Watch)
      {
        StepRecord *s = new (std::nothrow) StepRecord(ctx, argv[op == Watch ? 1 : 0]);
        if (!s)
          return JS_ThrowOutOfMemory(ctx);
        if (self->tail_)
          self->tail_->next = s;
        else
          self->head_ = s;
        self->tail_ = s;
        if (op == Watch)
          self->watched_ = JS_DupValue(ctx, argv[0]);
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
    enum BuilderMethod
    {
      Step,
      Named,
      Success,
      Failure,
      Watch
    };
    static JSClassID classId_;
    static JSClassID skipClassId_;
    JSRuntime *runtime_;
    StepRecord *head_, *tail_;
    JSValue success_, failure_, watched_;
    bool consumed_;
  };

} // namespace smirkycard
#endif
