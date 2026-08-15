#ifndef LOKA_MINESWEEPER_APP_CONFIG_HPP
#define LOKA_MINESWEEPER_APP_CONFIG_HPP

#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/WindowDefinition.hpp"
#include "MainNode.hpp"

class MyAppConfig : public AppConfigurable
{
public:
  MyAppConfig(PlatformContext *ctx, const minesweeper::MainProps &mainProps)
      : AppConfigurable(ctx),
        mainProps_(mainProps)
  {
  }

  virtual void compose(AppComposition &c)
  {
    c << WindowDef(WindowProps()
                       .frame(20, 20, 220, 240)
                       .scene(loka::app::scene::NodeDefinition<minesweeper::MainProps, minesweeper::MainNode>(
                           this->mainProps_))
                       .title("LokaMine")
                       .visible(true));
  }

private:
  minesweeper::MainProps mainProps_;
};

#endif // LOKA_MINESWEEPER_APP_CONFIG_HPP
