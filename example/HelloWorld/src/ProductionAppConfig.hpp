#ifndef LOKA_HELLOWORLD_PRODUCTION_APP_CONFIG_HPP
#define LOKA_HELLOWORLD_PRODUCTION_APP_CONFIG_HPP

#include "MyAppConfig.hpp"

#include <ctime>

/** Immutable startup fact derived from wall-clock seconds for HelloWorld's
    intentionally varying production menu order. Tests and non-production
    adapters may supply an explicit clock reading without changing the policy. */
class HelloWorldMenuSeed
{
public:
  static HelloWorldMenuSeed FromWallClock(std::time_t wallClockSeconds)
  {
    return HelloWorldMenuSeed(
        static_cast<unsigned long>(wallClockSeconds));
  }

  static HelloWorldMenuSeed ForProductionStartup()
  {
    return FromWallClock(std::time(0));
  }

  unsigned long value() const
  {
    return this->value_;
  }

private:
  explicit HelloWorldMenuSeed(unsigned long value)
      : value_(value)
  {
  }

  unsigned long value_;
};

/** Shared production bootstrap adapter for every HelloWorld platform entry
    point. The overload taking a completed seed keeps the pass-through policy
    directly verifiable without adding a test-only branch. */
class HelloWorldProductionAppConfig : public HelloWorldAppConfig
{
public:
  explicit HelloWorldProductionAppConfig(PlatformContext *context)
      : HelloWorldAppConfig(
            context,
            HelloWorldMenuSeed::ForProductionStartup().value())
  {
  }

  HelloWorldProductionAppConfig(PlatformContext *context,
                                const HelloWorldMenuSeed &menuSeed)
      : HelloWorldAppConfig(context, menuSeed.value())
  {
  }
};

#endif // LOKA_HELLOWORLD_PRODUCTION_APP_CONFIG_HPP
