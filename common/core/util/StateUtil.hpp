#ifndef LOKA_STATEUTIL_HPP
#define LOKA_STATEUTIL_HPP
#include <vector>
#include <cstdarg>

namespace loka
{
  namespace core
  {
    class StateBase;

    // Build a vector<StateBase*> using varargs (C++98-friendly). Use typedefs to avoid nested template closers.
    typedef StateBase StateBaseType;
    typedef std::vector<StateBaseType *> StateVector;

    static StateVector makeStateVector(StateBaseType *first, ...)
    {
      StateVector v;
      va_list args;
      va_start(args, first);
      for (StateBaseType *s = first; s != 0; s = va_arg(args, StateBaseType *))
      {
        if (s) // exclude null pointers as well as terminator
          v.push_back(s);
      }
      va_end(args);
      return v;
    }
  } // namespace core
} // namespace loka

// Null terminator for makeStateVector - must be properly typed for 64-bit varargs
#define STATE_NULL ((::loka::core::StateBaseType *)0)

#endif // LOKA_STATEUTIL_HPP
