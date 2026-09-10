#ifndef SMIRKYCARD_SCRIPT_RUNTIME_HPP
#define SMIRKYCARD_SCRIPT_RUNTIME_HPP

#include "ScriptEngine.h"

namespace smirkycard
{
  /** App-owned runtime; card boundaries borrow it and store no JS values. */
  class ScriptRuntime
  {
  public:
    ScriptRuntime()
        : script_(SmirkyScriptCreate())
    {
    }
    ~ScriptRuntime()
    {
      SmirkyScriptDestroy(this->script_);
    }

    SmirkyCardId evaluate(const char *source, char *error, size_t capacity)
    {
      return SmirkyScriptEvaluate(this->script_, source, error, capacity);
    }

  private:
    SmirkyScript *script_;
    ScriptRuntime(const ScriptRuntime &);
    ScriptRuntime &operator=(const ScriptRuntime &);
  };
} // namespace smirkycard
#endif
