#ifndef LOKA_FLOPPY_BIRD_GAME_MODEL_HPP
#define LOKA_FLOPPY_BIRD_GAME_MODEL_HPP

#include "GameLogic.hpp"
#include "MainNode.hpp"
#include "core/StateTracker.hpp"
#include "core/util/StateTrackerGuard.hpp"

namespace floppybird
{
  /** Owns one FloppyBird run and its rendered projection. The caller owns the
      seed and decides which frame deltas enter the fixed-step game logic. */
  class GameModel : public SharedModel
  {
  private:
    struct RenderSnapshot
    {
      loka_floppy_bird::GameState state;
      short score;
      short birdY;
      short pipeCount;
      short pipeLeft[loka_floppy_bird::kMaxPipes];
      short pipeGapTop[loka_floppy_bird::kMaxPipes];
      short pipeGapBottom[loka_floppy_bird::kMaxPipes];

      RenderSnapshot()
          : state(loka_floppy_bird::GAME_WAITING),
            score(0),
            birdY(0),
            pipeCount(0)
      {
        for (int i = 0; i < loka_floppy_bird::kMaxPipes; ++i)
        {
          this->pipeLeft[i] = 0;
          this->pipeGapTop[i] = 0;
          this->pipeGapBottom[i] = 0;
        }
      }

      bool operator==(const RenderSnapshot &other) const
      {
        if (this->state != other.state || this->score != other.score || this->birdY != other.birdY
            || this->pipeCount != other.pipeCount)
        {
          return false;
        }
        for (short i = 0; i < this->pipeCount; ++i)
        {
          if (this->pipeLeft[i] != other.pipeLeft[i] || this->pipeGapTop[i] != other.pipeGapTop[i]
              || this->pipeGapBottom[i] != other.pipeGapBottom[i])
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

  public:
    explicit GameModel(unsigned long seed)
        : SharedModel(),
          game_(),
          tracker_(),
          lastSnapshot_(),
          hasLastSnapshot_(false),
          cachedModel_()
    {
      this->tracker_.addState(&this->surfaceModel_);
      this->tracker_.addState(&this->scoreText_);
      this->game_.seed(seed);
      this->renderScene();
    }

    /** Starts a fresh deterministic run while preserving the model identity
        observed by the current or next Scene. */
    void reset(unsigned long seed)
    {
      loka::core::StateTrackerGuard guard(&this->tracker_);
      this->game_.reset(seed);
      this->lastSnapshot_ = RenderSnapshot();
      this->hasLastSnapshot_ = false;
      this->renderScene();
    }

    void advanceFrame(double frameSeconds)
    {
      loka::core::StateTrackerGuard guard(&this->tracker_);
      if (this->game_.advanceFrame(frameSeconds))
      {
        this->renderScene();
      }
    }

    void flap()
    {
      loka::core::StateTrackerGuard guard(&this->tracker_);
      this->game_.pressFlap();
      this->renderScene();
    }

    /** Reports the current game phase without exposing mutable GameLogic. */
    loka_floppy_bird::GameState gameState() const
    {
      return this->game_.state();
    }

    /** Reports the score owned by this run. */
    int score() const
    {
      return this->game_.score();
    }

  private:
    bool buildSnapshot(RenderSnapshot &snapshot)
    {
      snapshot.state = this->game_.state();
      snapshot.score = static_cast<short>(this->game_.score());
      snapshot.pipeCount = static_cast<short>(this->game_.pipeCount());
      snapshot.birdY = static_cast<short>(this->game_.birdY() + 0.5);
      for (short i = 0; i < snapshot.pipeCount; ++i)
      {
        const loka_floppy_bird::Pipe &pipe = this->game_.pipeAt(i);
        snapshot.pipeLeft[i] = static_cast<short>(pipe.x + 0.5);
        snapshot.pipeGapTop[i] =
            static_cast<short>(pipe.gapCenterY - loka_floppy_bird::kPipeGapHeight / 2);
        snapshot.pipeGapBottom[i] =
            static_cast<short>(pipe.gapCenterY + loka_floppy_bird::kPipeGapHeight / 2);
      }
      if (!this->hasLastSnapshot_ || snapshot != this->lastSnapshot_)
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
      const bool hadPreviousSnapshot = this->hasLastSnapshot_;
      const short previousScore = hadPreviousSnapshot ? this->lastSnapshot_.score : 0;
      const bool wasDead = hadPreviousSnapshot && this->lastSnapshot_.state == loka_floppy_bird::GAME_DEAD;
      if (!this->buildSnapshot(snapshot))
      {
        return;
      }
      const bool isDead = snapshot.state == loka_floppy_bird::GAME_DEAD;
      if (!hadPreviousSnapshot || previousScore != snapshot.score || wasDead != isDead)
      {
        if (isDead)
        {
          this->scoreText_.set(loka::core::String::Literal("Game Over - Score: ")
                               + loka::core::String::FromInt(snapshot.score));
        }
        else
        {
          this->scoreText_.set(loka::core::String::Literal("Score: ") + loka::core::String::FromInt(snapshot.score));
        }
      }

      this->cachedModel_.rectCount = 0;
      for (short i = 0; i < snapshot.pipeCount; ++i)
      {
        if (this->cachedModel_.rectCount < loka::app::RectSurfaceModel::kMaxRects)
        {
          this->cachedModel_.rects[this->cachedModel_.rectCount++] = loka::app::RectSprite(
              snapshot.pipeLeft[i],
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
                                    static_cast<short>(loka_floppy_bird::kWindowHeight
                                                       - snapshot.pipeGapBottom[i]));
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

      this->surfaceModel_.set(this->cachedModel_);
    }

    loka_floppy_bird::GameLogic game_;
    loka::core::PushStateTracker tracker_;
    RenderSnapshot lastSnapshot_;
    bool hasLastSnapshot_;
    loka::app::RectSurfaceModel cachedModel_;
  };
} // namespace floppybird

#endif // LOKA_FLOPPY_BIRD_GAME_MODEL_HPP
