#ifndef LOKA_FLOPPY_BIRD_GAME_LOGIC_HPP
#define LOKA_FLOPPY_BIRD_GAME_LOGIC_HPP

#include <math.h>

namespace loka_floppy_bird
{
  enum GameState
  {
    GAME_WAITING = 0,
    GAME_PLAYING = 1,
    GAME_DEAD = 2
  };

  static const int kWindowWidth = 320;
  static const int kWindowHeight = 240;
  static const int kBirdX = 72;
  static const int kBirdWidth = 18;
  static const int kBirdHeight = 14;
  static const int kPipeWidth = 24;
  static const int kPipeGapHeight = 72;
  static const int kMaxPipes = 6;

  static const double kGravity = 760.0;
  static const double kJumpVelocity = -250.0;
  static const double kPipeSpeed = 128.0;
  static const double kPipeInterval = 1.35;
  static const double kFirstPipeDelay = 0.70;
  static const double kFixedStepSeconds = 1.0 / 60.0;
  static const double kMaxFrameSeconds = 0.25;
  static const int kMaxStepsPerFrame = 8;

  struct Pipe
  {
    double x;
    int gapCenterY;
    bool counted;

    Pipe() : x(0.0), gapCenterY(kWindowHeight / 2), counted(false) {}
  };

  class GameLogic
  {
  public:
    GameLogic()
        : state_(GAME_PLAYING),
          birdY_(initialBirdY()),
          birdVelocity_(0.0),
          score_(0),
          pipeCount_(0),
          waitPhase_(0.0),
          nextPipeSeconds_(kFirstPipeDelay),
          randomState_(1UL),
          accumulatorSeconds_(0.0)
    {
    }

    void seed(unsigned long value)
    {
      this->randomState_ = value == 0UL ? 1UL : value;
    }

    void pressFlap()
    {
      if (this->state_ == GAME_DEAD)
      {
        this->resetForNewRun();
        this->state_ = GAME_PLAYING;
      }
      else if (this->state_ == GAME_WAITING)
      {
        this->state_ = GAME_PLAYING;
      }
      this->birdVelocity_ = kJumpVelocity;
    }

    bool advanceFrame(double frameSeconds)
    {
      if (frameSeconds < 0.0)
      {
        frameSeconds = 0.0;
      }
      if (frameSeconds > kMaxFrameSeconds)
      {
        frameSeconds = kMaxFrameSeconds;
      }

      this->accumulatorSeconds_ += frameSeconds;
      bool advanced = false;
      int steps = 0;
      while (this->accumulatorSeconds_ >= kFixedStepSeconds &&
             steps < kMaxStepsPerFrame)
      {
        this->step(kFixedStepSeconds);
        this->accumulatorSeconds_ -= kFixedStepSeconds;
        ++steps;
        advanced = true;
      }
      if (steps == kMaxStepsPerFrame &&
          this->accumulatorSeconds_ >= kFixedStepSeconds)
      {
        this->accumulatorSeconds_ = 0.0;
      }
      return advanced;
    }

    GameState state() const { return this->state_; }
    int score() const { return this->score_; }
    int pipeCount() const { return this->pipeCount_; }
    const Pipe &pipeAt(int index) const { return this->pipes_[index]; }

    double birdY() const
    {
      if (this->state_ == GAME_WAITING)
      {
        return initialBirdY() + sin(this->waitPhase_ * 6.28318530717958647692) * 7.0;
      }
      return this->birdY_;
    }

  private:
    static double initialBirdY()
    {
      return (kWindowHeight - kBirdHeight) * 0.5;
    }

    void resetForNewRun()
    {
      this->birdY_ = initialBirdY();
      this->birdVelocity_ = 0.0;
      this->score_ = 0;
      this->pipeCount_ = 0;
      this->waitPhase_ = 0.0;
      this->nextPipeSeconds_ = kFirstPipeDelay;
      this->accumulatorSeconds_ = 0.0;
    }

    void step(double dt)
    {
      if (this->state_ == GAME_WAITING)
      {
        this->waitPhase_ += dt;
        while (this->waitPhase_ >= 1.0)
        {
          this->waitPhase_ -= 1.0;
        }
        return;
      }

      if (this->state_ != GAME_PLAYING)
      {
        return;
      }

      this->birdVelocity_ += kGravity * dt;
      this->birdY_ += this->birdVelocity_ * dt;

      this->nextPipeSeconds_ -= dt;
      while (this->nextPipeSeconds_ <= 0.0)
      {
        this->spawnPipe();
        this->nextPipeSeconds_ += kPipeInterval;
      }

      for (int i = this->pipeCount_ - 1; i >= 0; --i)
      {
        Pipe &pipe = this->pipes_[i];
        pipe.x -= kPipeSpeed * dt;

        if (!pipe.counted &&
            pipe.x + kPipeWidth <= static_cast<double>(kBirdX))
        {
          pipe.counted = true;
          ++this->score_;
        }

        if (pipe.x + kPipeWidth < 0.0)
        {
          this->removePipe(i);
        }
      }

      if (this->birdY_ < 0.0 ||
          this->birdY_ + kBirdHeight > static_cast<double>(kWindowHeight))
      {
        if (this->birdY_ < 0.0)
        {
          this->birdY_ = 0.0;
        }
        else
        {
          this->birdY_ = static_cast<double>(kWindowHeight - kBirdHeight);
        }
        this->birdVelocity_ = 0.0;
      }

      const int birdTop = static_cast<int>(this->birdY_);
      const int birdBottom = birdTop + kBirdHeight;
      const int birdRight = kBirdX + kBirdWidth;
      for (int i = 0; i < this->pipeCount_; ++i)
      {
        const Pipe &pipe = this->pipes_[i];
        const int pipeLeft = static_cast<int>(pipe.x);
        const int pipeRight = pipeLeft + kPipeWidth;
        if (birdRight <= pipeLeft || kBirdX >= pipeRight)
        {
          continue;
        }
        const int gapTop = pipe.gapCenterY - kPipeGapHeight / 2;
        const int gapBottom = pipe.gapCenterY + kPipeGapHeight / 2;
        if (birdTop < gapTop || birdBottom > gapBottom)
        {
          continue;
        }
      }
    }

    void spawnPipe()
    {
      if (this->pipeCount_ >= kMaxPipes)
      {
        return;
      }

      const int minGapCenter = kPipeGapHeight / 2 + 16;
      const int maxGapCenter = kWindowHeight - (kPipeGapHeight / 2) - 16;
      int range = maxGapCenter - minGapCenter + 1;
      if (range < 1)
      {
        range = 1;
      }

      Pipe &pipe = this->pipes_[this->pipeCount_];
      pipe.x = static_cast<double>(kWindowWidth);
      pipe.gapCenterY = minGapCenter + nextRandomInt(range);
      pipe.counted = false;
      ++this->pipeCount_;
    }

    void removePipe(int index)
    {
      for (int i = index; i < this->pipeCount_ - 1; ++i)
      {
        this->pipes_[i] = this->pipes_[i + 1];
      }
      --this->pipeCount_;
    }

    int nextRandomInt(int range)
    {
      this->randomState_ = this->randomState_ * 1664525UL + 1013904223UL;
      return static_cast<int>(this->randomState_ % static_cast<unsigned long>(range));
    }

    GameState state_;
    double birdY_;
    double birdVelocity_;
    int score_;
    Pipe pipes_[kMaxPipes];
    int pipeCount_;
    double waitPhase_;
    double nextPipeSeconds_;
    unsigned long randomState_;
    double accumulatorSeconds_;
  };
}

#endif
