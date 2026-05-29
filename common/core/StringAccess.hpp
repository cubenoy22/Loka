#ifndef LOKA_CORE_STRING_ACCESS_HPP
#define LOKA_CORE_STRING_ACCESS_HPP

#include "core/String.hpp"

namespace loka
{
  namespace core
  {
    class StringAccess
    {
    public:
      static const loka::core::Managed<platform::String> &handle(const String &value)
      {
        return value.handle_;
      }
    };

  } // namespace core
} // namespace loka

#endif // LOKA_CORE_STRING_ACCESS_HPP
