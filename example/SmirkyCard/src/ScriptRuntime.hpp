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
  /** Built-in card definitions shared by the app bootstrap and tests. */
  const char *BuiltinMainJs();
  /** App-owned runtime; card boundaries borrow it and store no JS values. */
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
      explicit InterruptWindow(ScriptRuntime &runtime);
      ~InterruptWindow();

    private:
      ScriptRuntime &runtime_;
      InterruptWindow(const InterruptWindow &);
      InterruptWindow &operator=(const InterruptWindow &);
    };

    ScriptRuntime();
    ~ScriptRuntime()
    {
      this->closeEngine();
    }

    JSContext *context() const;
    JSRuntime *jsRuntime() const;
    void setActive(JsCardNode *node)
    {
      this->active_ = node;
    }
    JsCardNode *active() const
    {
      return this->active_;
    }
    JSValue constructorFor(SmirkyCardId id) const
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
    bool loadBuiltin(const char *source, loka::core::String &error);
    /** Selects MAIN.JS once through the application's portable file door. */
    void loadMain(PlatformContext *context);
    /** Replaces registered card constructors only after MAIN.JS validates. */
    bool reloadMain(loka::core::String &error);
    MainSource mainSource() const
    {
      return this->mainSource_;
    }
    /** A startup MAIN.JS failure belongs on the first card, or every card
        when the file reached QuickJS but could not evaluate. */
    loka::core::String mainErrorFor(SmirkyCardId card) const;
    /** Every JS invocation passes through one of these doors.  Success values
        are transferred to the caller, which must either own or free them. */
    bool callConstructor(JSValueConst ctor, JSValue &result, loka::core::String &error);
    bool call(JSValueConst fn,
              JSValueConst receiver,
              int argc,
              JSValueConst *argv,
              JSValue &result,
              loka::core::String &error);
    bool evalMain(const char *source, std::size_t length, const char *name, loka::core::String &error);
    /* Helper callback is public only so the local tree factory can install it. */
    static JSValue testId(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue enabled(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);
    static JSValue declare(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv);

    SmirkyCardId evaluate(const char *source, char *error, size_t capacity)
    {
      return SmirkyScriptEvaluate(this->script_, source, error, capacity);
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
      if (SmirkyScriptEvaluateToString(this->script_,
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

  private:
    friend class JsCardNode;
    friend class JsCardBindingRegistry;
    friend bool RegisterSmirkyCardBindings(JsCardBindingRegistry &registry);
    /** The engine (runtime + context + installed globals) is opened once at
        construction and again after a MAIN.JS that failed, so a script that
        poisoned a global before throwing never reaches the fallback cards. */
    void openEngine();
    void closeEngine();
    void openInterruptWindow();
    void closeInterruptWindow();
    static int interrupt(JSRuntime *, void *opaque);
    bool captureException(loka::core::String &error);
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
    SmirkyScript *script_;
    JSValue first_;
    JSValue second_;
    JsCardNode *active_;
    MainSource mainSource_;
    PlatformContext *mainContext_;
    loka::core::String mainError_;
    MainErrorScope mainErrorScope_;
    struct InterruptState
    {
      InterruptState()
          : depth(0),
            remaining(0)
      {
      }
      unsigned int depth;
      unsigned int remaining;
    } interrupts_;
    ScriptRuntime(const ScriptRuntime &);
    ScriptRuntime &operator=(const ScriptRuntime &);
  };
} // namespace smirkycard
#endif
