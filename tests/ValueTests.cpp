#include "ValueTests.hpp"

#include <cassert>

#include "loka/core/Value.hpp"

void testLokaValueCore()
{
  {
    loka::core::Array first;
    first.pushBack(loka::core::Value(1L));

    loka::core::Array second(first);
    second[0] = loka::core::Value(2L);

    assert(first[0].asInt(0) == 1);
    assert(second[0].asInt(0) == 2);
  }

  {
    loka::core::Dictionary first;
    first.set(loka::core::String::Literal("count"), loka::core::Value(1L));

    loka::core::Dictionary second(first);
    loka::core::Value *value = second.find(loka::core::String::Literal("count"));
    assert(value != 0);
    *value = loka::core::Value(2L);

    const loka::core::Value *firstValue = first.find(loka::core::String::Literal("count"));
    const loka::core::Value *secondValue = second.find(loka::core::String::Literal("count"));

    assert(firstValue != 0);
    assert(secondValue != 0);
    assert(firstValue->asInt(0) == 1);
    assert(secondValue->asInt(0) == 2);
  }
}
