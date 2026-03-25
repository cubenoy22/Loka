#ifndef LOKA_FLOPPY_BIRD_APP_CONFIG_HPP
#define LOKA_FLOPPY_BIRD_APP_CONFIG_HPP

#include "app/AppComposition.hpp"
#include "app/AppConfigurable.hpp"
#include "app/Menu.hpp"
#include "app/RectSurface.hpp"
#include "app/WindowDefinition.hpp"
#include "loka/core/StateTracker.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "MainNode.hpp"

class MyAppConfig : public AppConfigurable
{
public:
  struct RenderSnapshot
  {
    short birdY;
    short pipeCount;
    short pipeLeft[loka_floppy_bird::kMaxPipes];
    short pipeGapTop[loka_floppy_bird::kMaxPipes];
    short pipeGapBottom[loka_floppy_bird::kMaxPipes];

    RenderSnapshot()
        : birdY(0),
          pipeCount(0)
    {
      for (int i = 0; i < loka_floppy_bird::kMaxPipes; ++i)
      {
        pipeLeft[i] = 0;
        pipeGapTop[i] = 0;
        pipeGapBottom[i] = 0;
      }
    }

    bool operator==(const RenderSnapshot &other) const
    {
      if (birdY != other.birdY || pipeCount != other.pipeCount)
      {
        return false;
      }
      for (short i = 0; i < pipeCount; ++i)
      {
        if (pipeLeft[i] != other.pipeLeft[i] ||
            pipeGapTop[i] != other.pipeGapTop[i] ||
            pipeGapBottom[i] != other.pipeGapBottom[i])
        {
          return false;
        }
      }
      return true;
    }

    bool operator!=(const RenderSnapshot &other) const
    {
      return !(*this == other);
    }
  };

  explicit MyAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx),
        shared_(),
        game_(),
        tracker_(),
        lastSnapshot_(),
        hasLastSnapshot_(false),
        cachedModel_(),
        lastScore_(-1)
  {
    this->tracker_.addState(&this->shared_.surfaceModel_);
    this->tracker_.addState(&this->shared_.scoreText_);
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
    if (!this->game_.advanceFrame(elapsedSeconds))
    {
      return;
    }
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
  bool buildSnapshot(RenderSnapshot &snapshot)
  {
    snapshot.pipeCount = static_cast<short>(this->game_.pipeCount());
    snapshot.birdY = static_cast<short>(this->game_.birdY() + 0.5);
    for (short i = 0; i < snapshot.pipeCount; ++i)
    {
      const loka_floppy_bird::Pipe &pipe = this->game_.pipeAt(i);
      snapshot.pipeLeft[i] = static_cast<short>(pipe.x + 0.5);
      snapshot.pipeGapTop[i] = static_cast<short>(pipe.gapCenterY - loka_floppy_bird::kPipeGapHeight / 2);
      snapshot.pipeGapBottom[i] = static_cast<short>(pipe.gapCenterY + loka_floppy_bird::kPipeGapHeight / 2);
    }
    if (!hasLastSnapshot_ || snapshot != this->lastSnapshot_)
    {
      this->lastSnapshot_ = snapshot;
      this->hasLastSnapshot_ = true;
      return true;
    }
    return false;
  }

  void renderScene()
  {
    RenderSnapshot snapshot;
    const int score = this->game_.score();
    if (score != this->lastScore_)
    {
      this->shared_.scoreText_.set(loka::core::String::Literal("Score: ")
                                       + loka::core::String::FromInt(score));
      this->lastScore_ = score;
    }
    if (!this->buildSnapshot(snapshot))
    {
      return;
    }

    this->cachedModel_.rectCount = 0;
    for (short i = 0; i < snapshot.pipeCount; ++i)
    {
      if (this->cachedModel_.rectCount < loka::app::RectSurfaceModel::kMaxRects)
      {
        this->cachedModel_.rects[this->cachedModel_.rectCount++] =
            loka::app::RectSprite(snapshot.pipeLeft[i],
                                  0,
                                  static_cast<short>(loka_floppy_bird::kPipeWidth),
                                  snapshot.pipeGapTop[i]);
      }
      if (this->cachedModel_.rectCount < loka::app::RectSurfaceModel::kMaxRects)
      {
        this->cachedModel_.rects[this->cachedModel_.rectCount++] =
            loka::app::RectSprite(snapshot.pipeLeft[i],
                                  snapshot.pipeGapBottom[i],
                                  static_cast<short>(loka_floppy_bird::kPipeWidth),
                                  static_cast<short>(loka_floppy_bird::kWindowHeight - snapshot.pipeGapBottom[i]));
      }
    }

    if (this->cachedModel_.rectCount < loka::app::RectSurfaceModel::kMaxRects)
    {
      this->cachedModel_.rects[this->cachedModel_.rectCount++] =
          loka::app::RectSprite(static_cast<short>(loka_floppy_bird::kBirdX),
                                snapshot.birdY,
                                static_cast<short>(loka_floppy_bird::kBirdWidth),
                                static_cast<short>(loka_floppy_bird::kBirdHeight));
    }

    this->shared_.surfaceModel_.set(this->cachedModel_);
  }

  floppybird::SharedModel shared_;
  loka_floppy_bird::GameLogic game_;
  loka::core::PushStateTracker tracker_;
  RenderSnapshot lastSnapshot_;
  bool hasLastSnapshot_;
  loka::app::RectSurfaceModel cachedModel_;
  int lastScore_;
};

#endif
