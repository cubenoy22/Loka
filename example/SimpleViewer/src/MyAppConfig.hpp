#ifndef LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
#define LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP

#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "core/State.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "app/core/WindowDefinition.hpp"
#include "app/Menu.hpp"
#include "MainNode.hpp"

class SimpleViewerAppConfig : public AppConfigurable
{
public:
  explicit SimpleViewerAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx),
        openDialogEvent_(),
        menu_(&this->openDialogEvent_)
  {
  }

  virtual void compose(AppComposition &c)
  {
    c << WindowDef(
        WindowProps()
            .frame(40, 40, 320, 240)
            .scene(loka::app::scene::Boundary<simpleviewer::MainNode>(
                simpleviewer::MainProps()
                    .platformContext(this->getPlatformContext()) // TODO: Make this retrievable from inside the Node
                    .openDialogEvent(&this->openDialogEvent_)
                    .actualSize(this->menu_.actualSizeState())))
            .title("LokaSimpleViewer")
            .visible(true));
  }

  virtual void composeMenu(loka::app::MenuComposition &c)
  {
    c << this->menu_;
  }

private:
  class MainMenu : public loka::app::MenuBoundary
  {
  public:
    explicit MainMenu(loka::core::EmitterState *openDialogEvent)
        : openDialogEvent_(openDialogEvent),
          actualSize_(0),
          fitToWindowEvent_(),
          actualSizeEvent_()
    {
      this->reserveStates(1);
      // MenuBoundary's tracked-state door is explicit because this boundary
      // owns the mode while the scene receives only a read-only State view.
      this->actualSize_ = &this->dangerouslyUseState<bool>(false);
    }

    virtual void composeMenu(loka::app::MenuComposition &c)
    {
      using namespace loka::app;
      this->bindActionForMenu(this->fitToWindowEvent_, &MainMenu::fitToWindow);
      this->bindActionForMenu(this->actualSizeEvent_, &MainMenu::showActualSize);
      const bool actual = this->actualSize_->get();
      c.declare(AppMenu()                                              //
                << MenuItem("About").actionType(MENU_ACTION_ABOUT_APP) //
                << MenuSeparator()                                     //
                << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
      c.declare(Menu("File") //
                << MenuItem("Open...").onClick(this->openDialogEvent_));
      c.declare(Menu("View")
                << MenuItem("Fit to Window")
                       .attr(MenuItemAttr().checked(!actual))
                       .onClick(&this->fitToWindowEvent_)
                << MenuItem("Actual Size")
                       .attr(MenuItemAttr().checked(actual))
                       .onClick(&this->actualSizeEvent_));
    }

    loka::core::State<bool> *actualSizeState() const
    {
      return this->actualSize_;
    }

  private:
    void fitToWindow()
    {
      loka::core::StateTrackerGuard guard(this->tracker());
      this->actualSize_->set(false);
    }

    void showActualSize()
    {
      loka::core::StateTrackerGuard guard(this->tracker());
      this->actualSize_->set(true);
    }

    loka::core::EmitterState *openDialogEvent_;
    loka::core::MutableState<bool> *actualSize_;
    loka::core::EmitterState fitToWindowEvent_;
    loka::core::EmitterState actualSizeEvent_;
  };

  loka::core::EmitterState openDialogEvent_;
  MainMenu menu_;
};

#endif // LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
