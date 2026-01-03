#ifndef DECLARA_MY_APP_CONFIG_HPP
#define DECLARA_MY_APP_CONFIG_HPP

#include "core/AppComposition.hpp"
#include "core/AppConfigurable.hpp"
#include "core/WindowDefinition.hpp"
#include "BmiFormComponent.hpp"

class MyAppConfig : public AppConfigurable
{
public:
  explicit MyAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx) {}

  virtual void compose(AppComposition &c)
  {
    c.declare(WindowDef(WindowProps()
                            .scene(helloworld::BmiForm())
                            .title("LokaSample")
                            .visible(true)));
  }
};

#endif // DECLARA_MY_APP_CONFIG_HPP
