#ifndef LOKA_FLOPPY_BIRD_APP_CONFIG_HPP
#define LOKA_FLOPPY_BIRD_APP_CONFIG_HPP

#include "app/AppComposition.hpp"
#include "app/AppConfigurable.hpp"
#include "app/Menu.hpp"
#include "app/WindowDefinition.hpp"
#include "loka/core/StateTracker.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "MainNode.hpp"

class MyAppConfig : public AppConfigurable
{
public:
  explicit MyAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx),
        shared_(),
        game_(),
        tracker_()
  {
    this->tracker_.addState(&this->shared_.titleText_);
    this->tracker_.addState(&this->shared_.statusText_);
    this->tracker_.addState(&this->shared_.scoreText_);
    this->tracker_.addState(&this->shared_.surfaceModel_);
    this->game_.seed(1UL);
    this->renderScene();
  }

  virtual void compose(AppComposition &c)
  {
    c << WindowDef(WindowProps()
                       .frame(50, 50, 380, 340)
                       .scene(loka::app::scene::NodeDefinition<floppybird::MainProps, floppybird::MainNode>(
                           floppybird::MainProps(&this->shared_)))
                       .title("LokaFloppyBird")
                       .visible(true));
  }

  virtual void composeMenu(loka::app::MenuComposition &c)
  {
    using namespace loka::app;
    c.declare(AppMenu() << MenuItem("About").actionType(MENU_ACTION_ABOUT_APP)
                        << MenuSeparator()
                        << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
  }

  virtual bool wantsIdleUpdates() const
  {
    return true;
  }

  virtual void onIdle(double elapsedSeconds)
  {
    loka::core::StateTrackerGuard guard(&this->tracker_);
    this->game_.advanceFrame(elapsedSeconds);
    this->renderScene();
  }

  virtual bool handleKeyPress(char key)
  {
    if (key != ' ')
    {
      return false;
    }
    loka::core::StateTrackerGuard guard(&this->tracker_);
    this->game_.pressFlap();
    this->renderScene();
    return true;
  }

private:
  void renderScene()
  {
    this->shared_.scoreText_.set(loka::core::String::Literal("Score: ")
                                     + loka::core::String::FromInt(this->game_.score()),
                                 true);
    if (this->game_.state() == loka_floppy_bird::GAME_WAITING)
    {
      this->shared_.statusText_.set(loka::core::String::Literal("Press Space To Start"), true);
    }
    else if (this->game_.state() == loka_floppy_bird::GAME_PLAYING)
    {
      this->shared_.statusText_.set(loka::core::String::Literal("Space = Flap"), true);
    }
    else
    {
      this->shared_.statusText_.set(loka::core::String::Literal("Game Over - Press Space To Retry"), true);
    }

    loka::app::RectSurfaceModel model;
    for (int i = 0; i < this->game_.pipeCount(); ++i)
    {
      const loka_floppy_bird::Pipe &pipe = this->game_.pipeAt(i);
      const short pipeLeft = static_cast<short>(pipe.x + 0.5);
      const short gapTop = static_cast<short>(pipe.gapCenterY - loka_floppy_bird::kPipeGapHeight / 2);
      const short gapBottom = static_cast<short>(pipe.gapCenterY + loka_floppy_bird::kPipeGapHeight / 2);

      if (model.rectCount < loka::app::RectSurfaceModel::kMaxRects)
      {
        model.rects[model.rectCount++] =
            loka::app::RectSprite(pipeLeft,
                                  0,
                                  static_cast<short>(loka_floppy_bird::kPipeWidth),
                                  gapTop);
      }
      if (model.rectCount < loka::app::RectSurfaceModel::kMaxRects)
      {
        model.rects[model.rectCount++] =
            loka::app::RectSprite(pipeLeft,
                                  gapBottom,
                                  static_cast<short>(loka_floppy_bird::kPipeWidth),
                                  static_cast<short>(loka_floppy_bird::kWindowHeight - gapBottom));
      }
    }

    if (model.rectCount < loka::app::RectSurfaceModel::kMaxRects)
    {
      model.rects[model.rectCount++] =
          loka::app::RectSprite(static_cast<short>(loka_floppy_bird::kBirdX),
                                static_cast<short>(this->game_.birdY() + 0.5),
                                static_cast<short>(loka_floppy_bird::kBirdWidth),
                                static_cast<short>(loka_floppy_bird::kBirdHeight));
    }

    this->shared_.surfaceModel_.set(model, true);
  }

  floppybird::SharedModel shared_;
  loka_floppy_bird::GameLogic game_;
  loka::core::PushStateTracker tracker_;
};

#endif
