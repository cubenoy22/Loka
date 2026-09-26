#include "ScriptRuntime.hpp"
#include "JsOwnProperties.hpp"
#include "CardNodes.hpp"
#include "app/PlatformContext.hpp"
#include "core/io/File.hpp"
#include "platform/file/FileHandle.hpp"
#if defined(LOKA_RETRO68)
#include "ToolboxByteSource.hpp"
#else
#include "core/resource/lrpk/LrpkStdioByteSource.hpp"
#endif
#include <cassert>
#include <new>

namespace smirkycard
{
  const char *BuiltinMainJs()
  {
#include "BuiltinMainJs.inc"
  }
  namespace
  {
    JSValue copyTreeWithId(JSContext *ctx, JSValueConst source, JSValueConst id)
    {
      // Every modifier copies the whole node, including the other modifiers'
      // results ("testId", "enabledSeat") and the modifier functions
      // themselves, so TEST_ID and enabled chain in either order.
      JsOwnProperties names(ctx);
      if (!names.read(source, JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK | JS_GPN_ENUM_ONLY))
        return JS_EXCEPTION;
      JSValue copy = JS_NewObject(ctx);
      if (JS_IsException(copy))
        return copy;
      for (uint32_t i = 0; i < names.count(); ++i)
      {
        JSValue value = JS_GetProperty(ctx, source, names.atom(i));
        if (JS_IsException(value) || JS_DefinePropertyValue(ctx, copy, names.atom(i), value, JS_PROP_C_W_E) < 0)
        {
          JS_FreeValue(ctx, copy);
          return JS_EXCEPTION;
        }
      }
      if (JS_SetPropertyStr(ctx, copy, "TEST_ID", JS_NewCFunction(ctx, &ScriptRuntime::testId, "TEST_ID", 1)) < 0
          || (!JS_IsUndefined(id) && JS_SetPropertyStr(ctx, copy, "testId", JS_DupValue(ctx, id)) < 0))
      {
        JS_FreeValue(ctx, copy);
        return JS_EXCEPTION;
      }
      return copy;
    }
  } // namespace
  JsEngine::JsEngine(ScriptRuntime &runtime, const JsCardBindingRegistry &registry)
      : runtime_(&runtime),
        script_(SmirkyScriptCreate()),
        first_(JS_UNDEFINED),
        second_(JS_UNDEFINED),
        cardCount_(0),
        nextRetired_(0)
  {
    if (!this->script_)
      return;
    JS_SetContextOpaque(this->context(), &runtime);
    JS_SetRuntimeOpaque(this->jsRuntime(), this);
    if (!registry.install(this->context()))
    {
      SmirkyScriptDestroy(this->script_);
      this->script_ = 0;
    }
  }

  JsEngine::~JsEngine()
  {
    assert(!this->cardCount_);
    if (this->script_)
    {
      JS_FreeValue(this->context(), this->first_);
      JS_FreeValue(this->context(), this->second_);
    }
    SmirkyScriptDestroy(this->script_);
  }

  JSContext *JsEngine::context() const
  {
    return SmirkyScriptContext(this->script_);
  }

  JSRuntime *JsEngine::jsRuntime() const
  {
    return SmirkyScriptRuntime(this->script_);
  }

  JSValue JsEngine::constructorFor(SmirkyCardId id) const
  {
    switch (id)
    {
    case SMIRKY_CARD_ERROR:
      return JS_UNDEFINED;
    case SMIRKY_CARD_FIRST:
      return JS_DupValue(this->context(), this->first_);
    case SMIRKY_CARD_SECOND:
      return JS_DupValue(this->context(), this->second_);
    }
    return JS_UNDEFINED;
  }

  bool JsEngine::hasConstructor(SmirkyCardId id) const
  {
    switch (id)
    {
    case SMIRKY_CARD_ERROR:
      return false;
    case SMIRKY_CARD_FIRST:
      return !JS_IsUndefined(this->first_);
    case SMIRKY_CARD_SECOND:
      return !JS_IsUndefined(this->second_);
    }
    return false;
  }

  void JsEngine::markRetired()
  {
    assert(this->runtime_ && this->runtime_->currentEngine_ != this && !this->nextRetired_);
    this->nextRetired_ = this->runtime_->retiredEngines_;
    this->runtime_->retiredEngines_ = this;
    if (!this->cardCount_)
      this->runtime_->destroyRetiredEngine(this);
  }

  JsEngineRef::JsEngineRef(JsEngine *engine)
      : engine_(engine)
  {
    if (this->engine_)
      ++this->engine_->cardCount_;
  }

  JsEngineRef::~JsEngineRef()
  {
    this->release();
  }

  void JsEngineRef::release()
  {
    if (!this->engine_)
      return;
    JsEngine *engine = this->engine_;
    this->engine_ = 0;
    assert(engine->cardCount_);
    --engine->cardCount_;
    if (!engine->cardCount_ && engine->runtime_->currentEngine_ != engine)
      engine->runtime_->destroyRetiredEngine(engine);
  }

  ScriptRuntime::ScriptRuntime()
      : registry_(),
        currentEngine_(0),
        retiredEngines_(0),
        active_(0),
        mainSource_(MAIN_SOURCE_BUILTIN),
        mainContext_(0),
        mainError_(),
        mainErrorScope_(MAIN_ERROR_NONE)
  {
    if (RegisterSmirkyCardBindings(this->registry_))
      this->currentEngine_ = this->createEngine();
  }

  ScriptRuntime::~ScriptRuntime()
  {
    assert(!this->interrupts_.top);
    assert(!this->retiredEngines_ && "retired JS engines must outlive no cards at runtime teardown");
    delete this->currentEngine_;
  }

  JsEngine *ScriptRuntime::createEngine()
  {
    JsEngine *engine = new (std::nothrow) JsEngine(*this, this->registry_);
    if (engine && !engine->context())
    {
      delete engine;
      engine = 0;
    }
    return engine;
  }

  void ScriptRuntime::replaceCurrentEngine(JsEngine *engine)
  {
    assert(engine && engine != this->currentEngine_);
    JsEngine *old = this->currentEngine_;
    this->currentEngine_ = engine;
    if (old)
      old->markRetired();
  }

  void ScriptRuntime::destroyRetiredEngine(JsEngine *engine)
  {
    assert(engine && engine != this->currentEngine_ && !engine->cardCount_);
    JsEngine **entry = &this->retiredEngines_;
    while (*entry && *entry != engine)
      entry = &(*entry)->nextRetired_;
    assert(*entry == engine);
    *entry = engine->nextRetired_;
    engine->nextRetired_ = 0;
    delete engine;
  }
  int ScriptRuntime::interrupt(JSRuntime *, void *opaque)
  {
    unsigned int *remaining = static_cast<unsigned int *>(opaque);
    if (!*remaining)
      return 1;
    --*remaining;
    return 0;
  }

  ScriptRuntime::InterruptWindow::InterruptWindow(ScriptRuntime &runtime, JsEngine &engine)
      : runtime_(runtime),
        engine_(engine),
        previous_(0),
        installed_(false)
  {
    this->runtime_.openInterruptWindow(*this);
  }

  ScriptRuntime::InterruptWindow::~InterruptWindow()
  {
    this->runtime_.closeInterruptWindow(*this);
  }

  void ScriptRuntime::openInterruptWindow(InterruptWindow &window)
  {
    window.previous_ = this->interrupts_.top;
    if (!window.previous_)
      this->interrupts_.remaining = 100;
    window.installed_ = true;
    for (InterruptWindow *outer = window.previous_; outer; outer = outer->previous_)
      if (&outer->engine_ == &window.engine_)
      {
        window.installed_ = false;
        break;
      }
    this->interrupts_.top = &window;
    if (window.installed_)
    {
      JS_SetInterruptHandler(window.engine_.jsRuntime(), &ScriptRuntime::interrupt, &this->interrupts_.remaining);
    }
  }

  void ScriptRuntime::closeInterruptWindow(InterruptWindow &window)
  {
    assert(this->interrupts_.top == &window);
    if (window.installed_)
      JS_SetInterruptHandler(window.engine_.jsRuntime(), 0, 0);
    this->interrupts_.top = window.previous_;
  }

  bool ScriptRuntime::captureException(JsEngine &engine, loka::core::String &error)
  {
    JSContext *context = engine.context();
    JSValue exception = JS_GetException(context);
    size_t length = 0;
    const char *text = JS_ToCStringLen(context, &length, exception);
    if (text)
    {
      if (length > 511)
      {
        length = 511;
        while (length && (static_cast<unsigned char>(text[length]) & 0xc0u) == 0x80u)
          --length;
      }
      error = loka::core::String::Utf8(text, length);
      JS_FreeCString(context, text);
    }
    else
      error = loka::core::String::Literal("JavaScript interrupted.");
    JS_FreeValue(context, exception);
    return false;
  }

  bool ScriptRuntime::callConstructor(JsEngine &engine, JSValueConst ctor, JSValue &result, loka::core::String &error)
  {
    InterruptWindow interrupt(*this, engine);
    result = JS_CallConstructor(engine.context(), ctor, 0, 0);
    if (JS_IsException(result))
    {
      JS_FreeValue(engine.context(), result);
      result = JS_UNDEFINED;
      return this->captureException(engine, error);
    }
    error = loka::core::String();
    return true;
  }

  bool ScriptRuntime::call(JsEngine &engine,
                           JSValueConst fn,
                           JSValueConst receiver,
                           int argc,
                           JSValueConst *argv,
                           JSValue &result,
                           loka::core::String &error)
  {
    InterruptWindow interrupt(*this, engine);
    result = JS_Call(engine.context(), fn, receiver, argc, argv);
    if (JS_IsException(result))
    {
      JS_FreeValue(engine.context(), result);
      result = JS_UNDEFINED;
      return this->captureException(engine, error);
    }
    error = loka::core::String();
    return true;
  }

  bool ScriptRuntime::evalMain(
      JsEngine &engine, const char *source, std::size_t length, const char *name, loka::core::String &error)
  {
    InterruptWindow interrupt(*this, engine);
    JSValue result;
    result = JS_Eval(engine.context(), source, length, name, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result))
    {
      JS_FreeValue(engine.context(), result);
      return this->captureException(engine, error);
    }
    JS_FreeValue(engine.context(), result);
    error = loka::core::String();
    return true;
  }
  JSContext *ScriptRuntime::context() const
  {
    return this->currentEngine_ ? this->currentEngine_->context() : 0;
  }
  JSRuntime *ScriptRuntime::jsRuntime() const
  {
    return this->currentEngine_ ? this->currentEngine_->jsRuntime() : 0;
  }

  JsCardNode *ScriptRuntime::active(JSContext *context) const
  {
    return this->active_ && this->active_->engine_->context() == context ? this->active_ : 0;
  }

  JSValue ScriptRuntime::card(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
  {
    ScriptRuntime *self = static_cast<ScriptRuntime *>(JS_GetContextOpaque(ctx));
    JsEngine *engine = static_cast<JsEngine *>(JS_GetRuntimeOpaque(JS_GetRuntime(ctx)));
    size_t length = 0;
    const char *name;
    if (!self || !engine || engine->runtime_ != self || argc != 2 || !JS_IsString(argv[0])
        || !JS_IsFunction(ctx, argv[1]))
      return JS_ThrowTypeError(ctx, "card(name, Class) requires a name and class");
    name = JS_ToCStringLen(ctx, &length, argv[0]);
    if (!name)
      return JS_EXCEPTION;
    JSValue *slot = 0;
    if (length == 5 && !memcmp(name, "first", 5))
      slot = &engine->first_;
    if (length == 6 && !memcmp(name, "second", 6))
      slot = &engine->second_;
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
    JsCardNode *active = self ? self->active(ctx) : 0;
    if (!active || argc != 1)
      return JS_ThrowTypeError(ctx, "state() is only valid during card construction");
    return active->mintState(ctx, argv[0]);
  }

  JSValue ScriptRuntime::testId(JSContext *ctx, JSValueConst thisValue, int argc, JSValueConst *argv)
  {
    if (argc != 1 || !JS_IsString(argv[0]))
      return JS_ThrowTypeError(ctx, "TEST_ID(id) requires a string");
    JSValue copy = copyTreeWithId(ctx, thisValue, argv[0]);
    if (JS_IsException(copy))
      return copy;
    if (JS_FreezeObject(ctx, copy) < 0)
    {
      JS_FreeValue(ctx, copy);
      return JS_EXCEPTION;
    }
    return copy;
  }
  JSValue ScriptRuntime::enabled(JSContext *ctx, JSValueConst thisValue, int argc, JSValueConst *argv)
  {
    if (argc != 1)
      return JS_ThrowTypeError(ctx, "enabled(seat) requires one seat");
    JSValue copy = copyTreeWithId(ctx, thisValue, JS_UNDEFINED);
    if (JS_IsException(copy))
      return copy;
    if (JS_SetPropertyStr(ctx, copy, "enabledSeat", JS_DupValue(ctx, argv[0])) < 0 || JS_FreezeObject(ctx, copy) < 0)
    {
      JS_FreeValue(ctx, copy);
      return JS_EXCEPTION;
    }
    return copy;
  }

  JSValue ScriptRuntime::declare(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
  {
    ScriptRuntime *self = static_cast<ScriptRuntime *>(JS_GetContextOpaque(ctx));
    JsCardNode *active = self ? self->active(ctx) : 0;
    if (!active || argc != 1)
      return JS_ThrowTypeError(ctx, "declare(tree) requires an active card and tree");
    return active->setComposeTree(ctx, argv[0]) ? JS_UNDEFINED : JS_EXCEPTION;
  }

  JSValue ScriptRuntime::go(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv)
  {
    ScriptRuntime *self = static_cast<ScriptRuntime *>(JS_GetContextOpaque(ctx));
    JsCardNode *active = self ? self->active(ctx) : 0;
    size_t length = 0;
    const char *name;
    if (!active || argc != 1 || !JS_IsString(argv[0]))
      return JS_ThrowTypeError(ctx, "go(name) requires an active card and string name");
    name = JS_ToCStringLen(ctx, &length, argv[0]);
    if (!name)
      return JS_EXCEPTION;
    active->requestGo(name, length);
    JS_FreeCString(ctx, name);
    return JS_UNDEFINED;
  }

  JSValue ScriptRuntime::reload(JSContext *ctx, JSValueConst, int argc, JSValueConst *)
  {
    ScriptRuntime *self = static_cast<ScriptRuntime *>(JS_GetContextOpaque(ctx));
    JsCardNode *active = self ? self->active(ctx) : 0;
    if (!active || argc != 0)
      return JS_ThrowTypeError(ctx, "reload() requires an active card");
    active->requestReload();
    return JS_UNDEFINED;
  }

  bool ScriptRuntime::loadBuiltin(const char *source, loka::core::String &error)
  {
    if (!this->currentEngine_)
    {
      error = loka::core::String::Literal("JavaScript runtime is unavailable.");
      return false;
    }
    return this->evalMain(*this->currentEngine_, source, strlen(source), "BuiltinCards.js", error);
  }

  loka::core::String ScriptRuntime::mainErrorFor(SmirkyCardId card) const
  {
    if (this->mainErrorScope_ == MAIN_ERROR_EVERY_CARD
        || (this->mainErrorScope_ == MAIN_ERROR_FIRST_CARD && card == SMIRKY_CARD_FIRST))
      return this->mainError_;
    return loka::core::String();
  }

  void ScriptRuntime::loadMain(PlatformContext *context)
  {
    this->mainContext_ = context;
    this->mainSource_ = MAIN_SOURCE_BUILTIN;
    this->mainError_ = loka::core::String();
    this->mainErrorScope_ = MAIN_ERROR_NONE;
    if (!this->context())
      return; // unavailable runtime: every later call reports it (loadBuiltin's path)
    std::string text;
    loka::core::String error;
    if (this->readMain(text, error))
    {
      if (this->evalMain(*this->currentEngine_, text.data(), text.size(), "MAIN.JS", error))
      {
        this->mainSource_ = MAIN_SOURCE_FILE;
        return;
      }
      this->mainError_ =
          loka::core::String::Literal("MAIN.JS: ") + error + loka::core::String::Literal("; using built-in cards");
      this->mainErrorScope_ = MAIN_ERROR_EVERY_CARD;
      // The failed candidate may have redefined globals or registered cards;
      // built-ins run in a fresh engine, never the poisoned one.
      delete this->currentEngine_;
      this->currentEngine_ = this->createEngine();
      if (!this->currentEngine_)
        return;
    }
    else
    {
      this->mainError_ = error + loka::core::String::Literal("; using built-in cards");
      this->mainErrorScope_ = MAIN_ERROR_FIRST_CARD;
    }
    loka::core::String ignored;
    this->loadBuiltin(BuiltinMainJs(), ignored);
  }

  bool ScriptRuntime::readMain(std::string &text, loka::core::String &error) const
  {
    const loka::file::File item = loka::file::File::Application() << loka::file::File("MAIN.JS");
    loka::platform::file::FileHandle handle;
    if (!this->mainContext_ || !this->mainContext_->openFile(item, handle))
    {
      error = loka::core::String::Literal("MAIN.JS: file is missing");
      return false;
    }
#if defined(LOKA_RETRO68)
    loka::toolbox::ToolboxByteSource source;
    if (!handle.hasSpec || !source.open(handle.spec))
#else
    loka::core::resource::lrpk::StdioByteSource source;
    if (handle.displayPath.empty() || !source.open(handle.displayPath))
#endif
    {
      error = loka::core::String::Literal("MAIN.JS: file is missing or could not read");
      return false;
    }
    std::size_t length = 0;
    if (!source.size(length))
    {
      error = loka::core::String::Literal("MAIN.JS: could not read");
      return false;
    }
    else if (length > 64u * 1024u)
    {
      error = loka::core::String::Literal("MAIN.JS: exceeds 64 KiB");
      return false;
    }
    else
    {
      text.assign(length, '\0');
      if (!source.readAt(0, length ? reinterpret_cast<unsigned char *>(&text[0]) : 0, length))
      {
        error = loka::core::String::Literal("MAIN.JS: could not read");
        return false;
      }
      error = loka::core::String();
      return true;
    }
  }

  JsEngine *ScriptRuntime::prepareReload(SmirkyCardId card, loka::core::String &error)
  {
    std::string text;
    if (!this->readMain(text, error))
      return 0;
    JsEngine *candidate = this->createEngine();
    if (!candidate)
    {
      error = loka::core::String::Literal("MAIN.JS: JavaScript runtime is unavailable.");
      return 0;
    }
    if (!this->evalMain(*candidate, text.data(), text.size(), "MAIN.JS", error))
    {
      delete candidate;
      error = loka::core::String::Literal("MAIN.JS: ") + error;
      return 0;
    }
    if (!candidate->hasConstructor(card))
    {
      const char *name = card == SMIRKY_CARD_FIRST ? "first" : card == SMIRKY_CARD_SECOND ? "second" : "unknown";
      delete candidate;
      error = loka::core::String::Literal("MAIN.JS: card '") + loka::core::String::Literal(name)
              + loka::core::String::Literal("' is not defined");
      return 0;
    }
    error = loka::core::String();
    return candidate;
  }
  void ScriptRuntime::commitReload(JsEngine *candidate)
  {
    this->replaceCurrentEngine(candidate);
    this->mainSource_ = MAIN_SOURCE_FILE;
    this->mainError_ = loka::core::String();
    this->mainErrorScope_ = MAIN_ERROR_NONE;
  }
  void ScriptRuntime::discardReload(JsEngine *candidate)
  {
    delete candidate;
  }

#ifdef TEST_BUILD
  unsigned int ScriptRuntime::retiredEngineCount() const
  {
    unsigned int count = 0;
    for (JsEngine *engine = this->retiredEngines_; engine; engine = engine->nextRetired_)
      ++count;
    return count;
  }
#endif
} // namespace smirkycard
