#ifndef SMIRKYCARD_JS_NATIVE_CLASS_HPP
#define SMIRKYCARD_JS_NATIVE_CLASS_HPP
#include "quickjs.h"

namespace smirkycard
{
  /** Reserves a cached native class ID in every runtime before registering it.
      This QuickJS fork allocates IDs per runtime; JS_NewClass does not advance
      that allocator. Callers install classes in the same order in each engine. */
  inline bool RegisterJsNativeClass(JSRuntime *runtime, JSClassID &id, const JSClassDef &definition)
  {
    if (id && JS_IsRegisteredClass(runtime, id))
      return true;
    JSClassID reserved = 0;
    JS_NewClassID(runtime, &reserved);
    if (!id)
      id = reserved;
    if (reserved != id)
      return false;
    return JS_NewClass(runtime, id, &definition) == 0;
  }
} // namespace smirkycard
#endif
