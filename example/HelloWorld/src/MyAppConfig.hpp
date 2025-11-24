#ifndef DECLARA_MY_APP_CONFIG_HPP
#define DECLARA_MY_APP_CONFIG_HPP

#include "core/AppComponent.hpp"
#include "core/AppConfigurable.hpp"
#include "core/Window.hpp"
#include "HelloWorldComponent.hpp"

class MyAppConfig : public AppConfigurable
{
public:
  explicit MyAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx) {}

  virtual void configure(AppBuilder &builder)
  {
    builder.Window(
        new declara::core::scene::Scene(helloworld::HelloWorld()),
        WindowOptions()
            .setTitle("LokaSample")
            .setVisibility(true));
  }
};

#endif // DECLARA_MY_APP_CONFIG_HPP
