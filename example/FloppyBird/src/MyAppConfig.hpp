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
        renderedCells_()
  {
    this->tracker_.addState(&this->shared_.titleText_);
    this->tracker_.addState(&this->shared_.statusText_);
    this->tracker_.addState(&this->shared_.scoreText_);
    for (int i = 0; i < loka_floppy_bird::kBoardRows * loka_floppy_bird::kBoardCols; ++i)
    {
      this->tracker_.addState(&this->shared_.cells_[i]);
      this->renderedCells_[i] = '.';
    }
    this->game_.seed(1UL);
    this->renderScene();
  }

  virtual void compose(AppComposition &c)
  {
    c << WindowDef(WindowProps()
                       .frame(50, 50, 440, 420)
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

    char nextCells[loka_floppy_bird::kBoardRows * loka_floppy_bird::kBoardCols];
    for (int i = 0; i < loka_floppy_bird::kBoardRows * loka_floppy_bird::kBoardCols; ++i)
    {
      nextCells[i] = '.';
    }

    for (int i = 0; i < this->game_.pipeCount(); ++i)
    {
      const loka_floppy_bird::Pipe &pipe = this->game_.pipeAt(i);
      const int left = static_cast<int>(pipe.x + 0.5);
      for (int dx = 0; dx < loka_floppy_bird::kPipeWidthCols; ++dx)
      {
        const int col = left + dx;
        if (col < 0 || col >= loka_floppy_bird::kBoardCols)
        {
          continue;
        }
        for (int row = 0; row < loka_floppy_bird::kBoardRows; ++row)
        {
          if (row >= pipe.gapTopRow &&
              row < pipe.gapTopRow + loka_floppy_bird::kPipeGapRows)
          {
            continue;
          }
          nextCells[row * loka_floppy_bird::kBoardCols + col] = '#';
        }
      }
    }

    nextCells[this->game_.birdDisplayRow() * loka_floppy_bird::kBoardCols +
              loka_floppy_bird::kBirdColumn] = '@';

    for (int i = 0; i < loka_floppy_bird::kBoardRows * loka_floppy_bird::kBoardCols; ++i)
    {
      if (this->renderedCells_[i] == nextCells[i])
      {
        continue;
      }
      this->renderedCells_[i] = nextCells[i];
      if (nextCells[i] == '.')
      {
        this->shared_.cells_[i].set(loka::core::String::Literal(""), true);
      }
      else
      {
        char text[2];
        text[0] = nextCells[i];
        text[1] = 0;
        this->shared_.cells_[i].set(loka::core::String::Literal(text), true);
      }
    }
  }

  floppybird::SharedModel shared_;
  loka_floppy_bird::GameLogic game_;
  loka::core::PushStateTracker tracker_;
  char renderedCells_[loka_floppy_bird::kBoardRows * loka_floppy_bird::kBoardCols];
};

#endif
