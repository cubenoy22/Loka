#ifndef SMIRKYCARD_SCRIPT_ENGINE_H
#define SMIRKYCARD_SCRIPT_ENGINE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** The two C++ cards understood by this experiment; failure is not a card. */
  typedef enum SmirkyCardId
  {
    SMIRKY_CARD_ERROR,
    SMIRKY_CARD_FIRST,
    SMIRKY_CARD_SECOND
  } SmirkyCardId;

  /** Opaque, main-thread-only QuickJS runtime and context owned by the app. */
  typedef struct SmirkyScript SmirkyScript;
  SmirkyScript *SmirkyScriptCreate(void);
  void SmirkyScriptDestroy(SmirkyScript *script);

  /** Evaluates a synchronous script returning "first" or "second". Copies an
      error into the supplied buffer on failure; no JS value crosses this door. */
  SmirkyCardId SmirkyScriptEvaluate(SmirkyScript *script, const char *source, char *error, size_t errorCapacity);

  /** Evaluates a synchronous global script and copies its stringified result.
      On failure, copies the stringified exception to error. No JS value crosses
      this door. */
  int SmirkyScriptEvaluateToString(SmirkyScript *script, const char *source, char *result, size_t resultCapacity,
                                   char *error, size_t errorCapacity);

#ifdef __cplusplus
}
#endif
#endif
