#include "ScriptEngine.h"
#include "quickjs.h"
#include <stdlib.h>
#include <string.h>

struct SmirkyScript
{
  JSRuntime *runtime;
  JSContext *context;
};

static void copyError(char *out, size_t capacity, const char *message)
{
  if (out && capacity)
  {
    size_t length = strlen(message);
    if (length >= capacity)
      length = capacity - 1;
    memcpy(out, message, length);
    out[length] = '\0';
  }
}

static int copyJsString(JSContext *context, JSValue value, char *out, size_t capacity)
{
  size_t length;
  const char *text = JS_ToCStringLen(context, &length, value);
  if (!text)
    return 0;
  if (out && capacity)
  {
    if (length >= capacity)
      length = capacity - 1;
    memcpy(out, text, length);
    out[length] = '\0';
  }
  JS_FreeCString(context, text);
  return 1;
}

SmirkyScript *SmirkyScriptCreate(void)
{
  SmirkyScript *script = (SmirkyScript *)calloc(1, sizeof(*script));
  if (!script)
    return NULL;
  script->runtime = JS_NewRuntime();
  if (!script->runtime)
  {
    free(script);
    return NULL;
  }
#if defined(LOKA_RETRO68)
  /* Classic budget from the 68K engine probe on an 8 MB IIx (2026-09-15):
     S2 (200 objects, JSON round trip, closures) peaked at 291 KB of
     allocator payload, so 1 MiB leaves room for real card scripts; the
     32 KiB JS stack reached recursion depth 79 and 64 KiB reached 162. */
  JS_SetMemoryLimit(script->runtime, 1 * 1024 * 1024);
  JS_SetMaxStackSize(script->runtime, 64 * 1024);
#else
  JS_SetMemoryLimit(script->runtime, 8 * 1024 * 1024);
  JS_SetMaxStackSize(script->runtime, 256 * 1024);
#endif
  script->context = JS_NewContext(script->runtime);
  if (!script->context)
  {
    SmirkyScriptDestroy(script);
    return NULL;
  }
  return script;
}

void SmirkyScriptDestroy(SmirkyScript *script)
{
  if (!script)
    return;
  if (script->context)
    JS_FreeContext(script->context);
  JS_FreeRuntime(script->runtime);
  free(script);
}

/* Each evaluation owns its instruction budget on the stack. */
static int interruptScript(JSRuntime *runtime, void *opaque)
{
  unsigned int *remaining = (unsigned int *)opaque;
  (void)runtime;
  if (!*remaining)
    return 1;
  --*remaining;
  return 0;
}

/* Owns the interrupt window and any exception value. The caller owns a
   successful result and must JS_FreeValue it. */
static JSValue evaluateGlobal(SmirkyScript *script, const char *source, char *error, size_t errorCapacity,
                              int *succeeded)
{
  unsigned int remaining = 100;
  JSValue result;
  *succeeded = 0;
  copyError(error, errorCapacity, "");
  if (!script || !source)
  {
    copyError(error, errorCapacity, "JavaScript runtime is unavailable.");
    return JS_UNDEFINED;
  }
  JS_UpdateStackTop(script->runtime);
  JS_SetInterruptHandler(script->runtime, interruptScript, &remaining);
  result = JS_Eval(script->context, source, strlen(source), "card.js", JS_EVAL_TYPE_GLOBAL);
  JS_SetInterruptHandler(script->runtime, NULL, NULL);
  if (JS_IsException(result))
  {
    JSValue exception = JS_GetException(script->context);
    if (!copyJsString(script->context, exception, error, errorCapacity))
      copyError(error, errorCapacity, "JavaScript evaluation failed.");
    JS_FreeValue(script->context, exception);
    JS_FreeValue(script->context, result);
    return JS_UNDEFINED;
  }
  *succeeded = 1;
  return result;
}

SmirkyCardId SmirkyScriptEvaluate(SmirkyScript *script, const char *source, char *error, size_t errorCapacity)
{
  SmirkyCardId card = SMIRKY_CARD_ERROR;
  JSValue result;
  int succeeded;
  result = evaluateGlobal(script, source, error, errorCapacity, &succeeded);
  if (!succeeded)
    return card;
  if (JS_IsString(result))
  {
    size_t length;
    const char *name = JS_ToCStringLen(script->context, &length, result);
    if (name)
    {
      if (length == 5 && memcmp(name, "first", 5) == 0)
        card = SMIRKY_CARD_FIRST;
      else if (length == 6 && memcmp(name, "second", 6) == 0)
        card = SMIRKY_CARD_SECOND;
      JS_FreeCString(script->context, name);
    }
    if (card == SMIRKY_CARD_ERROR)
      copyError(error, errorCapacity, "Expected card name: first or second.");
  }
  else
  {
    copyError(error, errorCapacity, "The script must return a card name string.");
  }
  JS_FreeValue(script->context, result);
  return card;
}

int SmirkyScriptEvaluateToString(SmirkyScript *script, const char *source, char *result, size_t resultCapacity,
                                 char *error, size_t errorCapacity)
{
  JSValue value;
  int succeeded;
  copyError(result, resultCapacity, "");
  value = evaluateGlobal(script, source, error, errorCapacity, &succeeded);
  if (!succeeded)
    return 0;
  if (!copyJsString(script->context, value, result, resultCapacity))
  {
    JSValue exception = JS_GetException(script->context);
    if (!copyJsString(script->context, exception, error, errorCapacity))
      copyError(error, errorCapacity, "JavaScript result could not be converted to text.");
    JS_FreeValue(script->context, exception);
    JS_FreeValue(script->context, value);
    return 0;
  }
  JS_FreeValue(script->context, value);
  return 1;
}
