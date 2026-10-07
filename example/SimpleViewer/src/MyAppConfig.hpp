#ifndef LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
#define LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP

#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "core/State.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "app/core/WindowDefinition.hpp"
#include "app/Menu.hpp"
#include "MainNode.hpp"

#ifdef TEST_BUILD
class SimpleViewerTestAccess;
namespace simpleviewer { namespace testing { class MainAccess; } }
#endif

class SimpleViewerAppConfig : public AppConfigurable
{
public:
  explicit SimpleViewerAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx),
        openDialogEvent_()
  {
  }

  virtual void compose(AppComposition &c)
  {
    c << WindowDef(
        WindowProps()
            .frame(16, 16, 480, 280)
            .scene(loka::app::scene::Boundary<simpleviewer::MainNode>(
                simpleviewer::MainProps()
                    .platformContext(this->getPlatformContext()) // TODO: Make this retrievable from inside the Node
                    .openDialogEvent(&this->openDialogEvent_)))
            .title("LokaSimpleViewer")
            .visible(true));
  }

  virtual void composeMenu(loka::app::MenuComposition &) {}

private:
  loka::core::EmitterState openDialogEvent_;

#ifdef TEST_BUILD
  friend class simpleviewer::testing::MainAccess;
  friend class ::SimpleViewerTestAccess;
#endif
};

#endif // LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
