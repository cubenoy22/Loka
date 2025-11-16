#ifndef DECLARA_STATEUTIL_HPP
#define DECLARA_STATEUTIL_HPP
#include <vector>
#include <cstdarg>

// Forward declaration for StateBase
class StateBase;

// Build a vector<StateBase*> using varargs (C++98-friendly)
static std::vector<StateBase *> makeStateVector(StateBase *first, ...)
{
  std::vector<StateBase *> v;
  va_list args;
  va_start(args, first);
  for (StateBase *s = first; s != 0; s = va_arg(args, StateBase *))
  {
    if (s) // exclude null pointers as well as terminator
      v.push_back(s);
  }
  va_end(args);
  return v;
}

#endif // DECLARA_STATEUTIL_HPP
