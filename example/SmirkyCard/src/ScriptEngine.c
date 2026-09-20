#include "ScriptEngine.h"
#include "ScriptAlignedAlloc.h"
#include "quickjs.h"
#include <stdlib.h>
#include <string.h>

/* QuickJS receives 8-byte-aligned blocks on every rail; see ScriptAlignedAlloc.h. */
static const SmirkyBaseAllocator kScriptBaseAllocator = {malloc, free};

static void *ScriptCalloc(void *opaque, size_t count, size_t size)
{
  void *block;
  (void)opaque;
  if (count != 0 && size > (size_t)-1 / count)
    return NULL;
  block = SmirkyAlignedAllocate(&kScriptBaseAllocator, count * size);
  if (block)
    memset(block, 0, count * size);
  return block;
}

static void *ScriptMalloc(void *opaque, size_t size)
{
  (void)opaque;
  return SmirkyAlignedAllocate(&kScriptBaseAllocator, size);
}

static void ScriptFree(void *opaque, void *block)
{
  (void)opaque;
  SmirkyAlignedRelease(&kScriptBaseAllocator, block);
}

static void *ScriptRealloc(void *opaque, void *block, size_t size)
{
  (void)opaque;
  return SmirkyAlignedReallocate(&kScriptBaseAllocator, block, size);
}

static const JSMallocFunctions kScriptMallocFunctions = {
    ScriptCalloc, ScriptMalloc, ScriptFree, ScriptRealloc, SmirkyAlignedUsableSize};

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

/* Copies the value's text as UTF-8, capped at capacity-1 bytes and cut back
   to a code-point boundary, so a truncated result is still valid UTF-8.
   *outLength receives the copied byte count (embedded NULs are preserved).
   Runs user JavaScript (toString), so the caller keeps the interrupt handler
   installed around it. */
static int copyJsString(JSContext *context, JSValue value, char *out, size_t capacity, size_t *outLength)
{
  size_t length;
  const char *text = JS_ToCStringLen(context, &length, value);
  if (outLength)
    *outLength = 0;
  if (!text)
    return 0;
  if (out && capacity)
  {
    if (length >= capacity)
    {
      length = capacity - 1;
      while (length > 0 && (((unsigned char)text[length]) & 0xC0u) == 0x80u)
        --length;
    }
    memcpy(out, text, length);
    out[length] = '\0';
    if (outLength)
      *outLength = length;
  }
  JS_FreeCString(context, text);
  return 1;
}

SmirkyScript *SmirkyScriptCreate(void)
{
  SmirkyScript *script = (SmirkyScript *)calloc(1, sizeof(*script));
  if (!script)
    return NULL;
  script->runtime = JS_NewRuntime2(&kScriptMallocFunctions, NULL);
  if (!script->runtime)
  {
    free(script);
    return NULL;
  }
#if defined(LOKA_RETRO68)
  /* Classic budget, coupled to the SIZE partition (Size.r): a reload keeps
     two engines alive for one admission cycle, so the per-engine ceiling is
     half of what the partition can spare after CODE/DATA/UI (~0.8 MB):
     2 x 512 KiB + 0.8 MB fits the 2 MiB preferred size. The 68K engine
     probe on an 8 MB IIx (2026-09-15) peaked at 291 KB of allocator payload
     for S2 (200 objects, JSON round trip, closures), so 512 KiB still leaves
     room for real card scripts; a script that needs more gets QuickJS's
     out-of-memory exception (a refusal on the card, not a crash). The
     32 KiB JS stack reached recursion depth 79 and 64 KiB reached 162. */
  JS_SetMemoryLimit(script->runtime, 512 * 1024);
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

JSContext *SmirkyScriptContext(SmirkyScript *script)
{
  return script ? script->context : NULL;
}
JSRuntime *SmirkyScriptRuntime(SmirkyScript *script)
{
  return script ? script->runtime : NULL;
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

/* Evaluates under the caller's interrupt window (the caller installs the
   handler before and clears it after every stringification, since toString
   runs user JavaScript too). Owns any exception value; the caller owns a
   successful result and must JS_FreeValue it. */
static JSValue evaluateGlobal(
    SmirkyScript *script, const char *source, char *error, size_t errorCapacity, size_t *errorLength, int *succeeded)
{
  JSValue result;
  *succeeded = 0;
  copyError(error, errorCapacity, "");
  if (!script || !source)
  {
    copyError(error, errorCapacity, "JavaScript runtime is unavailable.");
    return JS_UNDEFINED;
  }
  JS_UpdateStackTop(script->runtime);
  result = JS_Eval(script->context, source, strlen(source), "card.js", JS_EVAL_TYPE_GLOBAL);
  if (JS_IsException(result))
  {
    JSValue exception = JS_GetException(script->context);
    if (!copyJsString(script->context, exception, error, errorCapacity, errorLength))
    {
      copyError(error, errorCapacity, "JavaScript evaluation failed.");
      if (errorLength)
        *errorLength = strlen(error);
    }
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
  unsigned int remaining = 100;
  if (!script || !source)
  {
    copyError(error, errorCapacity, "JavaScript runtime is unavailable.");
    return card;
  }
  JS_SetInterruptHandler(script->runtime, interruptScript, &remaining);
  result = evaluateGlobal(script, source, error, errorCapacity, NULL, &succeeded);
  if (succeeded)
  {
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
  }
  JS_SetInterruptHandler(script->runtime, NULL, NULL);
  return card;
}

int SmirkyScriptEvaluateToString(SmirkyScript *script,
                                 const char *source,
                                 char *result,
                                 size_t resultCapacity,
                                 size_t *resultLength,
                                 char *error,
                                 size_t errorCapacity,
                                 size_t *errorLength)
{
  JSValue value;
  int succeeded;
  int ok = 0;
  unsigned int remaining = 100;
  copyError(result, resultCapacity, "");
  if (resultLength)
    *resultLength = 0;
  if (errorLength)
    *errorLength = 0;
  if (!script || !source)
  {
    copyError(error, errorCapacity, "JavaScript runtime is unavailable.");
    if (errorLength)
      *errorLength = strlen(error);
    return 0;
  }
  /* One interrupt window covers evaluation and both stringifications: a
     result whose toString loops must be interrupted too, or Run hangs the
     main thread. */
  JS_SetInterruptHandler(script->runtime, interruptScript, &remaining);
  value = evaluateGlobal(script, source, error, errorCapacity, errorLength, &succeeded);
  if (succeeded)
  {
    if (copyJsString(script->context, value, result, resultCapacity, resultLength))
    {
      ok = 1;
    }
    else
    {
      JSValue exception = JS_GetException(script->context);
      if (!copyJsString(script->context, exception, error, errorCapacity, errorLength))
      {
        copyError(error, errorCapacity, "JavaScript result could not be converted to text.");
        if (errorLength)
          *errorLength = strlen(error);
      }
      JS_FreeValue(script->context, exception);
    }
    JS_FreeValue(script->context, value);
  }
  JS_SetInterruptHandler(script->runtime, NULL, NULL);
  return ok;
}
