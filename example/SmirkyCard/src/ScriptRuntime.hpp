#ifndef SMIRKYCARD_SCRIPT_RUNTIME_HPP
#define SMIRKYCARD_SCRIPT_RUNTIME_HPP

#include "ScriptEngine.h"
#include "JsCardBindingRegistry.hpp"
#include "quickjs.h"
#include "core/String.hpp"
#include <cstddef>
#include <cstring>
#include <string>

class PlatformContext;

namespace smirkycard
{
  class JsCardNode;
  class ScriptRuntime;

  /** One independently evaluated QuickJS generation. Cards retain the engine
      they were born in; reload overlap lasts at most the admission cycle in
      which the old Scene is retired. */
  class JsEngine
  {
  public:
    JSContext *context() const;
    JSRuntime *jsRuntime() const;
    JSValue constructorFor(SmirkyCardId id) const;
    bool hasConstructor(SmirkyCardId id) const;

  private:
    friend class JsEngineRef;
    friend class ScriptRuntime;
    JsEngine(ScriptRuntime &runtime, const JsCardBindingRegistry &registry);
    ~JsEngine();
    void markRetired();
    ScriptRuntime *runtime_;
    SmirkyScript *script_;
    JSValue first_;
    JSValue second_;
    unsigned int cardCount_;
    JsEngine *nextRetired_;
    JsEngine(const JsEngine &);
    JsEngine &operator=(const JsEngine &);
  };

  /** The only writer of JsEngine's live-card ledger. */
  class JsEngineRef
  {
  public:
    explicit JsEngineRef(JsEngine *engine = 0);
    ~JsEngineRef();

  private:
    void release();
    JsEngine *engine_;
    JsEngineRef(const JsEngineRef &);
    JsEngineRef &operator=(const JsEngineRef &);
  };

  /** Built-in card definitions shared by the app bootstrap and tests. */
  const char *BuiltinMainJs();
  /** App-owned runtime; card boundaries borrow it while retaining their birth
      engine through JsEngineRef. */
  class ScriptRuntime
  {
  public:
    enum MainSource
    {
      MAIN_SOURCE_BUILTIN,
      MAIN_SOURCE_FILE
    };
    /** Keeps QuickJS interruption enabled across a complete JS-facing operation. */
    class InterruptWindow
    {
    public:
      InterruptWindow(ScriptRuntime &runtime, JsEngine &engine);
      ~InterruptWindow();

    private:
      friend class ScriptRuntime;
      ScriptRuntime &runtime_;
      JsEngine &engine_;
      InterruptWindow *previous_;
      bool installed_;
      InterruptWindow(const InterruptWindow &);
      InterruptWindow &operator=(const InterruptWindow &);
    };

    ScriptRuntime();
    ~ScriptRuntime();

    JsEngine *currentEngine() const
    {
      return this->currentEngine_;
    }
    JSContext *context() const;
    JSRuntime *jsRuntime() const;
    void setActive(JsCardNode *node)
    {
      this->active_ = node;
    }
    JsCardNode *active(JSContext *context) const;
    bool loadBuiltin(const char *source, loka::core::String &error);
    /** Selects MAIN.JS once through the application's portable file door. */
    void loadMain(PlatformContext *context);
    /** Admits one evaluated engine only when it defines the requested card. */
    bool reloadMain(SmirkyCardId card, loka::core::String &error);
    MainSource mainSource() const
    {
      return this->mainSource_;
    }
    /** A startup MAIN.JS failure belongs on the first card, or every card
        when the file reached QuickJS but could not evaluate. */
    loka::core::String mainErrorFor(SmirkyCardId card) const;
    /** Every JS invocation passes through one of these doors. Success values
        are transferred to the caller, which must either own or free them. */
    bool callConstructor(JsEngine &engine, JSValueConst ctor, JSValue &result, loka::core::String &error);
    bool call(JsEngine &engine,
              JSValueConst fn,
              JSValueConst receiver,
              int argc,
              JSValueConst *argv,
              JSValue &result,
              loka::core::String &error);
    bool
    evalMain(JsEngine &engine, const char *source, std::size_t length, const char *name, loka::core::String &error);
    /* Helper callback is public only so the local tree factory can install it. */
    static JSValue testId(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue enabled(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue declare(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);

    SmirkyCardId evaluate(const char *source, char *error, size_t capacity)
    {
      return SmirkyScriptEvaluate(this->currentEngine_ ? this->currentEngine_->script_ : 0, source, error, capacity);
    }

    /** Evaluates UTF-8 source. Result and error are capped at 511 UTF-8 bytes. */
    bool evaluateToString(const loka::core::String &source, loka::core::String &result, loka::core::String &error)
    {
      char resultBuffer[512];
      char errorBuffer[512];
      const loka::core::StringBuffer sourceBuffer = source.bufferWithEncoding(loka::core::StringEncodingUtf8);
      const char *sourceBytes = static_cast<const char *>(sourceBuffer.data());
      const std::string sourceUtf8(sourceBytes ? sourceBytes : "", sourceBuffer.length());
      size_t resultLength = 0;
      size_t errorLength = 0;
      if (SmirkyScriptEvaluateToString(this->currentEngine_ ? this->currentEngine_->script_ : 0,
                                       sourceUtf8.c_str(),
                                       resultBuffer,
                                       sizeof(resultBuffer),
                                       &resultLength,
                                       errorBuffer,
                                       sizeof(errorBuffer),
                                       &errorLength))
      {
        result = loka::core::String::Utf8(resultBuffer, resultLength);
        error = loka::core::String();
        return true;
      }
      result = loka::core::String();
      error = loka::core::String::Utf8(errorBuffer, errorLength);
      return false;
    }

#ifdef TEST_BUILD
    unsigned int retiredEngineCount() const;
#endif

  private:
    friend class JsCardNode;
    friend class JsEngine;
    friend class JsEngineRef;
    friend class JsCardBindingRegistry;
    friend bool RegisterSmirkyCardBindings(JsCardBindingRegistry &registry);
    JsEngine *createEngine();
    void replaceCurrentEngine(JsEngine *engine);
    void destroyRetiredEngine(JsEngine *engine);
    void openInterruptWindow(InterruptWindow &window);
    void closeInterruptWindow(InterruptWindow &window);
    static int interrupt(JSRuntime *, void *opaque);
    bool captureException(JsEngine &engine, loka::core::String &error);
    static JSValue card(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue state(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue go(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue reload(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    bool readMain(std::string &text, loka::core::String &error) const;
    enum MainErrorScope
    {
      MAIN_ERROR_NONE,
      MAIN_ERROR_FIRST_CARD,
      MAIN_ERROR_EVERY_CARD
    };
    JsCardBindingRegistry registry_;
    JsEngine *currentEngine_;
    JsEngine *retiredEngines_;
    JsCardNode *active_;
    MainSource mainSource_;
    PlatformContext *mainContext_;
    loka::core::String mainError_;
    MainErrorScope mainErrorScope_;
    struct InterruptState
    {
      InterruptState()
          : top(0),
            remaining(0)
      {
      }
      InterruptWindow *top;
      unsigned int remaining;
    } interrupts_;
    ScriptRuntime(const ScriptRuntime &);
    ScriptRuntime &operator=(const ScriptRuntime &);
  };
} // namespace smirkycard
#endif
