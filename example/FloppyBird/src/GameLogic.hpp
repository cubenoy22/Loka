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

  static const int kBoardRows = 16;
  static const int kBoardCols = 22;
  static const int kPipeGapRows = 4;
  static const int kPipeWidthCols = 2;
  static const int kMaxPipes = 6;
  static const int kBirdColumn = 5;

  static const double kGravity = 18.0;
  static const double kJumpVelocity = -6.0;
  static const double kPipeSpeed = 6.0;
  static const double kPipeInterval = 1.55;
  static const double kFixedStepSeconds = 1.0 / 30.0;
  static const double kMaxFrameSeconds = 0.25;
  static const int kMaxStepsPerFrame = 8;

  struct Pipe
  {
    double x;
    int gapTopRow;
    bool counted;

    Pipe() : x(0.0), gapTopRow(0), counted(false) {}
  };

  class GameLogic
  {
  public:
    GameLogic()
        : state_(GAME_WAITING),
          birdRow_(initialBirdRow()),
          birdVelocity_(0.0),
          score_(0),
          pipeCount_(0),
          waitPhase_(0.0),
          nextPipeSeconds_(kPipeInterval),
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

    void advanceFrame(double frameSeconds)
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
      int steps = 0;
      while (this->accumulatorSeconds_ >= kFixedStepSeconds &&
             steps < kMaxStepsPerFrame)
      {
        this->step(kFixedStepSeconds);
        this->accumulatorSeconds_ -= kFixedStepSeconds;
        ++steps;
      }
      if (steps == kMaxStepsPerFrame &&
          this->accumulatorSeconds_ >= kFixedStepSeconds)
      {
        this->accumulatorSeconds_ = 0.0;
      }
    }

    GameState state() const { return this->state_; }
    int score() const { return this->score_; }
    int pipeCount() const { return this->pipeCount_; }
    const Pipe &pipeAt(int index) const { return this->pipes_[index]; }

    int birdDisplayRow() const
    {
      double value = this->birdRow_;
      if (this->state_ == GAME_WAITING)
      {
        value = initialBirdRow() + sin(this->waitPhase_ * 6.28318530717958647692) * 0.45;
      }
      int row = static_cast<int>(value + 0.5);
      if (row < 0)
      {
        row = 0;
      }
      if (row >= kBoardRows)
      {
        row = kBoardRows - 1;
      }
      return row;
    }

  private:
    static double initialBirdRow()
    {
      return (kBoardRows - 1) * 0.5;
    }

    void resetForNewRun()
    {
      this->birdRow_ = initialBirdRow();
      this->birdVelocity_ = 0.0;
      this->score_ = 0;
      this->pipeCount_ = 0;
      this->waitPhase_ = 0.0;
      this->nextPipeSeconds_ = kPipeInterval;
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
      this->birdRow_ += this->birdVelocity_ * dt;

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
            pipe.x + kPipeWidthCols <= static_cast<double>(kBirdColumn))
        {
          pipe.counted = true;
          ++this->score_;
        }

        if (pipe.x + kPipeWidthCols < 0.0)
        {
          this->removePipe(i);
        }
      }

      if (this->birdRow_ < 0.0 ||
          this->birdRow_ > static_cast<double>(kBoardRows - 1))
      {
        this->state_ = GAME_DEAD;
        return;
      }

      const int birdRow = this->birdDisplayRow();
      for (int i = 0; i < this->pipeCount_; ++i)
      {
        const Pipe &pipe = this->pipes_[i];
        const int pipeLeft = static_cast<int>(pipe.x);
        const int pipeRight = pipeLeft + kPipeWidthCols - 1;
        if (kBirdColumn < pipeLeft || kBirdColumn > pipeRight)
        {
          continue;
        }
        if (birdRow < pipe.gapTopRow ||
            birdRow >= pipe.gapTopRow + kPipeGapRows)
        {
          this->state_ = GAME_DEAD;
          return;
        }
      }
    }

    void spawnPipe()
    {
      if (this->pipeCount_ >= kMaxPipes)
      {
        return;
      }

      const int minGapTop = 2;
      const int maxGapTop = kBoardRows - kPipeGapRows - 2;
      int range = maxGapTop - minGapTop + 1;
      if (range < 1)
      {
        range = 1;
      }

      Pipe &pipe = this->pipes_[this->pipeCount_];
      pipe.x = static_cast<double>(kBoardCols);
      pipe.gapTopRow = minGapTop + nextRandomInt(range);
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
    double birdRow_;
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
