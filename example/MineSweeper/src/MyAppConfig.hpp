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
    loka::app::scene::NodeDefinition<minesweeper::MainProps, minesweeper::MainNode> mainDefinition(
        this->mainProps_);
    c << WindowDef(this->productionWindowProps(mainDefinition));
  }

protected:
  /** Declares MineSweeper's production window presentation around a supplied
      scene so non-production vehicles cannot drift its title or frame. */
  WindowProps productionWindowProps(const loka::app::scene::NodeDefinitionBase &scene) const
  {
    return WindowProps()
        .frame(20, 20, 220, 240)
        .scene(scene)
        .title("LokaMine")
        .visible(true);
  }

  const minesweeper::MainProps &mainProps() const
  {
    return this->mainProps_;
  }

private:
  minesweeper::MainProps mainProps_;
};

#endif // LOKA_MINESWEEPER_APP_CONFIG_HPP
