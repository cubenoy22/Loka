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
  JS_SetMemoryLimit(script->runtime, 8 * 1024 * 1024);
  JS_SetMaxStackSize(script->runtime, 256 * 1024);
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

SmirkyCardId SmirkyScriptEvaluate(SmirkyScript *script, const char *source, char *error, size_t errorCapacity)
{
  unsigned int remaining = 100;
  SmirkyCardId card = SMIRKY_CARD_ERROR;
  JSValue result;
  copyError(error, errorCapacity, "");
  if (!script || !source)
  {
    copyError(error, errorCapacity, "JavaScript runtime is unavailable.");
    return card;
  }
  JS_UpdateStackTop(script->runtime);
  JS_SetInterruptHandler(script->runtime, interruptScript, &remaining);
  result = JS_Eval(script->context, source, strlen(source), "card.js", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result))
  {
    JSValue exception = JS_GetException(script->context);
    const char *message = JS_ToCString(script->context, exception);
    copyError(error, errorCapacity, message ? message : "JavaScript evaluation failed.");
    if (message)
      JS_FreeCString(script->context, message);
    JS_FreeValue(script->context, exception);
  }
  else if (JS_IsString(result))
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
  JS_SetInterruptHandler(script->runtime, NULL, NULL);
  return card;
}
