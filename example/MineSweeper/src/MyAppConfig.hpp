#ifndef LOKA_MINESWEEPER_APP_CONFIG_HPP
#define LOKA_MINESWEEPER_APP_CONFIG_HPP

#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/WindowDefinition.hpp"
#include "MainNode.hpp"

class MyAppConfig : public AppConfigurable
{
public:
  explicit MyAppConfig(PlatformContext *ctx) : AppConfigurable(ctx) {}

  virtual void compose(AppComposition &c)
  {
    c << WindowDef(WindowProps()
                       .frame(20, 20, 220, 240)
                       .scene(loka::app::scene::NodeDefinition<minesweeper::MainProps, minesweeper::MainNode>())
                       .title("LokaMine")
                       .visible(true));
  }
};

#endif // LOKA_MINESWEEPER_APP_CONFIG_HPP
