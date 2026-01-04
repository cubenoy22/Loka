#ifndef LOKA_MY_APP_CONFIG_HPP
#define LOKA_MY_APP_CONFIG_HPP

#include "core/AppComposition.hpp"
#include "core/AppConfigurable.hpp"
#include "core/WindowDefinition.hpp"
#include "MainComponent.hpp"

class MyAppConfig : public AppConfigurable
{
public:
  explicit MyAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx) {}

  virtual void compose(AppComposition &c)
  {
    c.declare(WindowDef(WindowProps()
                            .scene(helloworld::Main())
                            .title("LokaSample")
                            .visible(true)));
  }
};

#endif // LOKA_MY_APP_CONFIG_HPP
