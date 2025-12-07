#ifndef DECLARA_HELLOWORLD_CONTEXT_HPP
#define DECLARA_HELLOWORLD_CONTEXT_HPP

#include <string>
#include "core/State.hpp"
#include "core2/scene/ContextDefinition.hpp"

namespace helloworld
{
  struct HelloWorldContext
  {
    declara::core::MutableState<std::string> message;
    HelloWorldContext() : message("Hello, Declara!") {}
  };

  inline declara::core::scene::ContextDefinition<HelloWorldContext> &HelloWorldContextDefinition()
  {
    static declara::core::scene::ContextDefinition<HelloWorldContext> def;
    return def;
  }

} // namespace helloworld

#endif // DECLARA_HELLOWORLD_CONTEXT_HPP
