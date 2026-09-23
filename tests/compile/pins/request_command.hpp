#ifndef LOKA_TEST_COMPILE_REQUEST_COMMAND_HPP
#define LOKA_TEST_COMPILE_REQUEST_COMMAND_HPP
#include "app/scene/node/ComposableNode.hpp"
struct TestCommand
{
  int value;
  TestCommand() : value(0) {}
  static TestCommand None() { return TestCommand(); }
  bool isNone() const { return this->value == 0; }
  bool operator!=(const TestCommand &other) const { return this->value != other.value; }
};
struct UntypedCommand : TestCommand
{
  static UntypedCommand None() { return UntypedCommand(); }
};
namespace loka { namespace app { namespace scene {
template <> struct RequestTraits<TestCommand> { enum { coalescable = 0 }; };
} } }
#endif
