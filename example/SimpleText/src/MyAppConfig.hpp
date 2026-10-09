#ifndef LOKA_SIMPLE_TEXT_APP_CONFIG_HPP
#define LOKA_SIMPLE_TEXT_APP_CONFIG_HPP

#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/WindowDefinition.hpp"
#include "MainNode.hpp"

class SimpleTextAppConfig : public AppConfigurable
{
public:
  explicit SimpleTextAppConfig(PlatformContext *context)
      : AppConfigurable(context)
  {
  }
  virtual void compose(AppComposition &c)
  {
    using namespace simpletext;
    c << WindowDef( //
        WindowProps()
            .frame(16, 16, 480, 320)
            .title("LokaSimpleText")
            .visible(true)
            .scene(loka::app::scene::Boundary<MainNode>( //
                MainProps().platformContext(this->getPlatformContext()) //
                )) //
    );
  }
};
#endif
