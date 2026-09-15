#ifndef SMIRKYCARD_SCRIPT_RUNTIME_HPP
#define SMIRKYCARD_SCRIPT_RUNTIME_HPP

#include "ScriptEngine.h"
#include "core/String.hpp"
#include <cstring>
#include <string>

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
      if (SmirkyScriptEvaluateToString(this->script_, sourceUtf8.c_str(), resultBuffer, sizeof(resultBuffer),
                                       &resultLength, errorBuffer, sizeof(errorBuffer), &errorLength))
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
    SmirkyScript *script_;
    ScriptRuntime(const ScriptRuntime &);
    ScriptRuntime &operator=(const ScriptRuntime &);
  };
} // namespace smirkycard
#endif
