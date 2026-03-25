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
        tracker_(),
        previousSurfaceModel_(),
        hasPreviousSurfaceModel_(false)
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
  static bool collectModelBounds(const loka::app::RectSurfaceModel &model,
                                 short &left,
                                 short &top,
                                 short &right,
                                 short &bottom)
  {
    if (model.rectCount <= 0)
    {
      left = top = right = bottom = 0;
      return false;
    }
    left = model.rects[0].x;
    top = model.rects[0].y;
    right = static_cast<short>(model.rects[0].x + model.rects[0].width);
    bottom = static_cast<short>(model.rects[0].y + model.rects[0].height);
    for (short i = 1; i < model.rectCount; ++i)
    {
      const loka::app::RectSprite &sprite = model.rects[i];
      if (sprite.x < left)
      {
        left = sprite.x;
      }
      if (sprite.y < top)
      {
        top = sprite.y;
      }
      if (sprite.x + sprite.width > right)
      {
        right = static_cast<short>(sprite.x + sprite.width);
      }
      if (sprite.y + sprite.height > bottom)
      {
        bottom = static_cast<short>(sprite.y + sprite.height);
      }
    }
    return true;
  }

  static void applyDirtyHint(loka::app::RectSurfaceModel &model,
                             const loka::app::RectSurfaceModel *previousModel,
                             short maxWidth,
                             short maxHeight)
  {
    short left = 0;
    short top = 0;
    short right = 0;
    short bottom = 0;
    bool hasBounds = collectModelBounds(model, left, top, right, bottom);
    if (previousModel)
    {
      short prevLeft = 0;
      short prevTop = 0;
      short prevRight = 0;
      short prevBottom = 0;
      if (collectModelBounds(*previousModel, prevLeft, prevTop, prevRight, prevBottom))
      {
        if (!hasBounds)
        {
          left = prevLeft;
          top = prevTop;
          right = prevRight;
          bottom = prevBottom;
          hasBounds = true;
        }
        else
        {
          if (prevLeft < left)
          {
            left = prevLeft;
          }
          if (prevTop < top)
          {
            top = prevTop;
          }
          if (prevRight > right)
          {
            right = prevRight;
          }
          if (prevBottom > bottom)
          {
            bottom = prevBottom;
          }
        }
      }
    }
    if (!hasBounds)
    {
      model.setDirtyRect(0, 0, 0, 0);
      return;
    }
    if (left < 0)
    {
      left = 0;
    }
    if (top < 0)
    {
      top = 0;
    }
    if (right > maxWidth)
    {
      right = maxWidth;
    }
    if (bottom > maxHeight)
    {
      bottom = maxHeight;
    }
    model.setDirtyRect(left, top, static_cast<short>(right - left), static_cast<short>(bottom - top));
  }

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

    applyDirtyHint(model,
                   this->hasPreviousSurfaceModel_ ? &this->previousSurfaceModel_ : 0,
                   static_cast<short>(loka_floppy_bird::kWindowWidth),
                   static_cast<short>(loka_floppy_bird::kWindowHeight));

    this->shared_.surfaceModel_.set(model, true);
    this->previousSurfaceModel_ = model;
    this->hasPreviousSurfaceModel_ = true;
  }

  floppybird::SharedModel shared_;
  loka_floppy_bird::GameLogic game_;
  loka::core::PushStateTracker tracker_;
  loka::app::RectSurfaceModel previousSurfaceModel_;
  bool hasPreviousSurfaceModel_;
};

#endif
