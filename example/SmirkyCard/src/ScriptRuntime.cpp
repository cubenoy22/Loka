#include "ScriptRuntime.hpp"
#include "CardNodes.hpp"

namespace smirkycard
{
  const char *BuiltinMainJs()
  {
    // Kept as one readable JavaScript text (C++98: adjacent literals, one
    // source line per literal) so the built-in cards double as the sample.
    return "// MAIN.JS — the built-in cards. Each card is a class: the constructor\n"
           "// declares its seats with state(initial); compose() returns the card's\n"
           "// tree using the Loka Node DSL names.\n"
           "card('first', class {\n"
           "  constructor() {\n"
           "    this.script = state('1+1');\n"
           "    this.result = state('Ready');\n"
           "  }\n"
           "  compose() {\n"
           "    return VStack(\n"
           "      Text('Card One').TEST_ID('SmirkyCard.Title'),\n"
           "      Text('This Scene is defined in JavaScript.'),\n"
           "      EditText(this.script).TEST_ID('SmirkyCard.Script'),\n"
           "      Button('Run', () => {\n"
           "        try { this.result.set(String(eval(this.script.get()))); }\n"
           "        catch (e) { this.result.set('Error: ' + e); }\n"
           "      }).TEST_ID('SmirkyCard.RunScript'),\n"
           "      Text(this.result).TEST_ID('SmirkyCard.Result'),\n"
           "      Button('Run JavaScript', () => go('second')).TEST_ID('SmirkyCard.Run'),\n"
           "      Text(this.error).TEST_ID('SmirkyCard.Status')\n"
           "    );\n"
           "  }\n"
           "});\n"
           "card('second', class {\n"
           "  constructor() {\n"
           "    this.script = state('2+2');\n"
           "    this.result = state('Ready');\n"
           "  }\n"
           "  compose() {\n"
           "    return VStack(\n"
           "      Text('Card Two').TEST_ID('SmirkyCard.Title'),\n"
           "      Text('This Scene is defined in JavaScript.'),\n"
           "      EditText(this.script).TEST_ID('SmirkyCard.Script'),\n"
           "      Row(\n"
           "        Button('Run', () => {\n"
           "          try { this.result.set(String(eval(this.script.get()))); }\n"
           "          catch (e) { this.result.set('Error: ' + e); }\n"
           "        }).TEST_ID('SmirkyCard.RunScript'),\n"
           "        Text(this.result).TEST_ID('SmirkyCard.Result'),\n"
           "        Button('Run JavaScript', () => go('first')).TEST_ID('SmirkyCard.Run')\n"
           "      ),\n"
           "      Text(this.error).TEST_ID('SmirkyCard.Status')\n"
           "    );\n"
           "  }\n"
           "});\n"
           "\n";
  }
  namespace
  {
    JSValue copyTreeWithId(JSContext *ctx, JSValueConst source, JSValueConst id)
    {
      // Every modifier copies the whole node, including the other modifiers'
      // results ("testId", "enabledSeat") and the modifier functions
      // themselves, so TEST_ID and enabled chain in either order.
      const char *names[] = {
          "kind", "children", "text", "seat", "label", "handler", "testId", "enabledSeat", "enabled"};
      JSValue copy = JS_NewObject(ctx);
      for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
      {
        JSValue value = JS_GetPropertyStr(ctx, source, names[i]);
        if (!JS_IsUndefined(value))
          JS_SetPropertyStr(ctx, copy, names[i], value);
        else
          JS_FreeValue(ctx, value);
      }
      JS_SetPropertyStr(ctx, copy, "TEST_ID", JS_NewCFunction(ctx, &ScriptRuntime::testId, "TEST_ID", 1));
      if (!JS_IsUndefined(id))
        JS_SetPropertyStr(ctx, copy, "testId", JS_DupValue(ctx, id));
      return copy;
    }
  } // namespace
  ScriptRuntime::ScriptRuntime()
      : registry_(),
        script_(SmirkyScriptCreate()),
        first_(JS_UNDEFINED),
        second_(JS_UNDEFINED),
        active_(0)
  {
    if (this->script_)
    {
      JS_SetContextOpaque(this->context(), this);
      if (!RegisterSmirkyCardBindings(this->registry_) || !this->registry_.install(this->context()))
        return;
      loka::core::String ignored;
      this->loadBuiltin(BuiltinMainJs(), ignored);
    }
  }
  int ScriptRuntime::interrupt(JSRuntime *, void *opaque)
  {
    unsigned int *remaining = static_cast<unsigned int *>(opaque);
    if (!*remaining)
      return 1;
    --*remaining;
    return 0;
  }

  ScriptRuntime::InterruptWindow::InterruptWindow(ScriptRuntime &runtime)
      : runtime_(runtime)
  {
    this->runtime_.openInterruptWindow();
  }

  ScriptRuntime::InterruptWindow::~InterruptWindow()
  {
    this->runtime_.closeInterruptWindow();
  }

  void ScriptRuntime::openInterruptWindow()
  {
    if (!this->interrupts_.depth++)
    {
      this->interrupts_.remaining = 100;
      JS_SetInterruptHandler(this->jsRuntime(), &ScriptRuntime::interrupt, &this->interrupts_.remaining);
    }
  }

  void ScriptRuntime::closeInterruptWindow()
  {
    assert(this->interrupts_.depth);
    if (!--this->interrupts_.depth)
      JS_SetInterruptHandler(this->jsRuntime(), 0, 0);
  }

  bool ScriptRuntime::captureException(loka::core::String &error)
  {
    JSValue exception = JS_GetException(this->context());
    size_t length = 0;
    const char *text = JS_ToCStringLen(this->context(), &length, exception);
    if (text)
    {
      if (length > 511)
      {
        length = 511;
        while (length && (static_cast<unsigned char>(text[length]) & 0xc0u) == 0x80u)
          --length;
      }
      error = loka::core::String::Utf8(text, length);
      JS_FreeCString(this->context(), text);
    }
    else
      error = loka::core::String::Literal("JavaScript interrupted.");
    JS_FreeValue(this->context(), exception);
    return false;
  }

  bool ScriptRuntime::callConstructor(JSValueConst ctor, JSValue &result, loka::core::String &error)
  {
    InterruptWindow interrupt(*this);
    result = JS_CallConstructor(this->context(), ctor, 0, 0);
    if (JS_IsException(result))
    {
      JS_FreeValue(this->context(), result);
      result = JS_UNDEFINED;
      return this->captureException(error);
    }
    error = loka::core::String();
    return true;
  }

  bool ScriptRuntime::call(
      JSValueConst fn, JSValueConst receiver, int argc, JSValueConst *argv, JSValue &result, loka::core::String &error)
  {
    InterruptWindow interrupt(*this);
    result = JS_Call(this->context(), fn, receiver, argc, argv);
    if (JS_IsException(result))
    {
      JS_FreeValue(this->context(), result);
      result = JS_UNDEFINED;
      return this->captureException(error);
    }
    error = loka::core::String();
    return true;
  }

  bool ScriptRuntime::evalBuiltin(const char *source, loka::core::String &error)
  {
    InterruptWindow interrupt(*this);
    JSValue result;
    result = JS_Eval(this->context(), source, strlen(source), "BuiltinCards.js", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result))
    {
      JS_FreeValue(this->context(), result);
      return this->captureException(error);
    }
    JS_FreeValue(this->context(), result);
    error = loka::core::String();
    return true;
  }
  JSContext *ScriptRuntime::context() const
  {
    return SmirkyScriptContext(this->script_);
  }
  JSRuntime *ScriptRuntime::jsRuntime() const
  {
    return SmirkyScriptRuntime(this->script_);
  }

  JSValue ScriptRuntime::card(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
  {
    ScriptRuntime *self = static_cast<ScriptRuntime *>(JS_GetContextOpaque(ctx));
    size_t length = 0;
    const char *name;
    if (!self || argc != 2 || !JS_IsString(argv[0]) || !JS_IsFunction(ctx, argv[1]))
      return JS_ThrowTypeError(ctx, "card(name, Class) requires a name and class");
    name = JS_ToCStringLen(ctx, &length, argv[0]);
    if (!name)
      return JS_EXCEPTION;
    JSValue *slot = 0;
    if (length == 5 && !memcmp(name, "first", 5))
      slot = &self->first_;
    if (length == 6 && !memcmp(name, "second", 6))
      slot = &self->second_;
    JS_FreeCString(ctx, name);
    if (!slot)
      return JS_ThrowRangeError(ctx, "unknown card name");
    JS_FreeValue(ctx, *slot);
    *slot = JS_DupValue(ctx, argv[1]);
    return JS_UNDEFINED;
  }

  JSValue ScriptRuntime::state(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
  {
    ScriptRuntime *self = static_cast<ScriptRuntime *>(JS_GetContextOpaque(ctx));
    if (!self || !self->active() || argc != 1)
      return JS_ThrowTypeError(ctx, "state() is only valid during card construction");
    return self->active()->mintState(ctx, argv[0]);
  }

  JSValue ScriptRuntime::testId(JSContext *ctx, JSValueConst thisValue, int argc, JSValueConst *argv)
  {
    if (argc != 1 || !JS_IsString(argv[0]))
      return JS_ThrowTypeError(ctx, "TEST_ID(id) requires a string");
    JSValue copy = copyTreeWithId(ctx, thisValue, argv[0]);
    JS_FreezeObject(ctx, copy);
    return copy;
  }
  JSValue ScriptRuntime::enabled(JSContext *ctx, JSValueConst thisValue, int argc, JSValueConst *argv)
  {
    if (argc != 1)
      return JS_ThrowTypeError(ctx, "enabled(seat) requires one seat");
    JSValue copy = copyTreeWithId(ctx, thisValue, JS_UNDEFINED);
    JS_SetPropertyStr(ctx, copy, "enabledSeat", JS_DupValue(ctx, argv[0]));
    JS_FreezeObject(ctx, copy);
    return copy;
  }

  JSValue ScriptRuntime::declare(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
  {
    ScriptRuntime *self = static_cast<ScriptRuntime *>(JS_GetContextOpaque(ctx));
    if (!self || !self->active() || argc != 1)
      return JS_ThrowTypeError(ctx, "declare(tree) requires an active card and tree");
    return self->active()->setComposeTree(ctx, argv[0]) ? JS_UNDEFINED : JS_EXCEPTION;
  }

  JSValue ScriptRuntime::go(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
  {
    ScriptRuntime *self = static_cast<ScriptRuntime *>(JS_GetContextOpaque(ctx));
    size_t length = 0;
    const char *name;
    if (!self || !self->active() || argc != 1 || !JS_IsString(argv[0]))
      return JS_ThrowTypeError(ctx, "go(name) requires an active card and string name");
    name = JS_ToCStringLen(ctx, &length, argv[0]);
    if (!name)
      return JS_EXCEPTION;
    self->active()->requestGo(name, length);
    JS_FreeCString(ctx, name);
    return JS_UNDEFINED;
  }

  bool ScriptRuntime::loadBuiltin(const char *source, loka::core::String &error)
  {
    if (!this->context())
    {
      error = loka::core::String::Literal("JavaScript runtime is unavailable.");
      return false;
    }
    return this->evalBuiltin(source, error);
  }
} // namespace smirkycard
