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
#endif

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
            .frame(16, 16, 480, 280)
            .scene(loka::app::scene::Boundary<simpleviewer::MainNode>(
                simpleviewer::MainProps()
                    .platformContext(this->getPlatformContext()) // TODO: Make this retrievable from inside the Node
                    .openDialogEvent(&this->openDialogEvent_)
                    .displayMode(this->menu_.displayModeState())
                    .fitEvent(this->menu_.fitEvent())
                    .actualEvent(this->menu_.actualEvent())
                    .actualScrollEvent(this->menu_.actualScrollEvent())))
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
          displayMode_(0),
          fitToWindowEvent_(),
          actualEvent_(),
          actualScrollEvent_()
    {
      this->reserveStates(1);
      // MenuBoundary's tracked-state door is explicit because this boundary
      // owns the mode while the scene receives only a read-only State view.
      this->displayMode_ =
          &this->dangerouslyUseState<int>(simpleviewer::DISPLAY_FIT);
    }

    virtual void composeMenu(loka::app::MenuComposition &c)
    {
      using namespace loka::app;
      this->bindActionForMenu(this->fitToWindowEvent_, &MainMenu::fitToWindow);
      this->bindActionForMenu(this->actualEvent_, &MainMenu::showActual);
      this->bindActionForMenu(this->actualScrollEvent_, &MainMenu::showActualScrolling);
      const int mode = this->displayMode_->get();
      c.declare(AppMenu()                                              //
                << MenuItem("About").actionType(MENU_ACTION_ABOUT_APP) //
                << MenuSeparator()                                     //
                << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
      c.declare(Menu("File") //
                << MenuItem("Open...").onClick(this->openDialogEvent_));
      c.declare(Menu("View")
                << MenuItem("Fit to Window")
                       .attr(MenuItemAttr().checked(mode == simpleviewer::DISPLAY_FIT))
                       .onClick(&this->fitToWindowEvent_)
                << MenuItem("Actual Size")
                       .attr(MenuItemAttr().checked(mode == simpleviewer::DISPLAY_ACTUAL))
                       .onClick(&this->actualEvent_)
                << MenuItem("Actual Size (Scroll)")
                       .attr(MenuItemAttr().checked(mode == simpleviewer::DISPLAY_ACTUAL_SCROLL))
                       .onClick(&this->actualScrollEvent_));
    }

    loka::core::State<int> *displayModeState() const
    {
      return this->displayMode_;
    }

    loka::core::EmitterState *fitEvent()
    {
      return &this->fitToWindowEvent_;
    }

    loka::core::EmitterState *actualEvent()
    {
      return &this->actualEvent_;
    }

    loka::core::EmitterState *actualScrollEvent()
    {
      return &this->actualScrollEvent_;
    }

  private:
#ifdef TEST_BUILD
    friend class ::SimpleViewerTestAccess;
#endif

    void setDisplayMode(simpleviewer::DisplayMode mode)
    {
      loka::core::StateTrackerGuard guard(this->tracker());
      this->displayMode_->set(static_cast<int>(mode));
    }

    void fitToWindow()
    {
      this->setDisplayMode(simpleviewer::DISPLAY_FIT);
    }

    void showActual()
    {
      this->setDisplayMode(simpleviewer::DISPLAY_ACTUAL);
    }

    void showActualScrolling()
    {
      this->setDisplayMode(simpleviewer::DISPLAY_ACTUAL_SCROLL);
    }

    loka::core::EmitterState *openDialogEvent_;
    loka::core::MutableState<int> *displayMode_;
    loka::core::EmitterState fitToWindowEvent_;
    loka::core::EmitterState actualEvent_;
    loka::core::EmitterState actualScrollEvent_;
  };

  loka::core::EmitterState openDialogEvent_;
  MainMenu menu_;

#ifdef TEST_BUILD
  friend class ::SimpleViewerTestAccess;
#endif
};

#endif // LOKA_SIMPLE_VIEWER_APP_CONFIG_HPP
