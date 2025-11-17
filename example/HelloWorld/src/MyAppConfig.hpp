#ifndef DECLARA_MY_APP_CONFIG_HPP
#define DECLARA_MY_APP_CONFIG_HPP

#include "core/AppComponent.hpp"
#include "core/AppConfigurable.hpp"
#include "core/Window.hpp"
#include "FormScene.hpp"

class MyAppConfig : public AppConfigurable
{
public:
  explicit MyAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx) {}

  virtual void configure(AppBuilder &builder)
  {
    builder.Window(
        new FormScene(),
        WindowOptions()
            .setTitle("DEVELOPERS!")
            .setVisibility(true));
  }
};

#endif // DECLARA_MY_APP_CONFIG_HPP
