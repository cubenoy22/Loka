#ifndef LOKA_SMIRK_BENCH_SMIRK_MODEL_HPP
#define LOKA_SMIRK_BENCH_SMIRK_MODEL_HPP

#include <assert.h>
#include <limits.h>
#include "app/RectSurface.hpp"
#include "core/StateTracker.hpp"
#include "core/util/StateTrackerGuard.hpp"

namespace smirkbench
{
  static const double kFixedStepSeconds = 1.0 / 60.0;
  static const double kMaxFrameSeconds = 0.25;
  static const int kMaxStepsPerFrame = 8;

  /** Owns the deterministic smirk simulation and its one RectSurface
      projection. Window geometry enters only through updateBounds(). */
  class SmirkModel
  {
  private:
    struct Face
    {
      Face()
          : sprite(),
            velocityX(0),
            velocityY(0)
      {
      }

      loka::app::RectSprite sprite;
      short velocityX;
      short velocityY;
    };

    class SpawnRandom
    {
    public:
      explicit SpawnRandom(unsigned long seed)
          : state_(seed)
      {
      }

      unsigned long next()
      {
        this->state_ = (this->state_ * 1664525UL + 1013904223UL) & 0xFFFFFFFFUL;
        return this->state_;
      }

      int nextIndex(int upperBound)
      {
        if (upperBound <= 0)
        {
          return 0;
        }
        return static_cast<int>((this->next() >> 16) % static_cast<unsigned long>(upperBound));
      }

    private:
      unsigned long state_;
    };

  public:
    enum
    {
      kFaceSize = 24
    };

    explicit SmirkModel(short width = 640, short height = 400, bool addInitialFace = true)
        : tracker_(),
          surfaceModel_(),
          faces_(),
          faceCount_(0),
          boundsWidth_(this->clampWall(width)),
          boundsHeight_(this->clampWall(height)),
          accumulatedSeconds_(0.0),
          cachedModel_()
    {
      this->tracker_.addState(&this->surfaceModel_);
      if (addInitialFace)
      {
        this->addFace();
      }
    }

    /** Adds one deterministically spawned face. False is the typed refusal
        once RectSurface's fixed capacity has been reached. */
    bool addFace()
    {
      if (this->faceCount_ >= loka::app::RectSurfaceModel::kMaxRects)
      {
        return false;
      }
      loka::core::StateTrackerGuard guard(&this->tracker_);
      this->faces_[this->faceCount_] = this->spawnFace(this->faceCount_);
      ++this->faceCount_;
      this->publishSurface();
      return true;
    }

    /** Advances only whole 60 Hz steps and retains the fractional remainder. */
    void advanceFrame(double elapsedSeconds)
    {
      if (elapsedSeconds < 0.0)
      {
        elapsedSeconds = 0.0;
      }
      if (elapsedSeconds > kMaxFrameSeconds)
      {
        elapsedSeconds = kMaxFrameSeconds;
      }
      this->accumulatedSeconds_ += elapsedSeconds;
      if (this->accumulatedSeconds_ < kFixedStepSeconds)
      {
        return;
      }

      loka::core::StateTrackerGuard guard(&this->tracker_);
      bool changed = false;
      int steps = 0;
      while (this->accumulatedSeconds_ >= kFixedStepSeconds && steps < kMaxStepsPerFrame)
      {
        this->stepFaces();
        this->accumulatedSeconds_ -= kFixedStepSeconds;
        ++steps;
        changed = true;
      }
      if (steps == kMaxStepsPerFrame && this->accumulatedSeconds_ >= kFixedStepSeconds)
      {
        this->accumulatedSeconds_ = 0.0;
      }
      if (changed)
      {
        this->publishSurface();
      }
    }

    /** Assigns Window geometry its app meaning as bounce walls and reclamps
        every face before publishing the new surface model. */
    /** The bounce walls take a laid-out seat (int Frame axes from the rail)
        and clamp it into sprite range: nothing below zero, nothing past
        SHRT_MAX. */
    void updateBounds(int width, int height)
    {
      const short nextWidth = this->clampWall(width);
      const short nextHeight = this->clampWall(height);
      if (this->boundsWidth_ == nextWidth && this->boundsHeight_ == nextHeight)
      {
        return;
      }
      this->boundsWidth_ = nextWidth;
      this->boundsHeight_ = nextHeight;
      loka::core::StateTrackerGuard guard(&this->tracker_);
      for (short i = 0; i < this->faceCount_; ++i)
      {
        this->clampFace(this->faces_[i]);
      }
      this->publishSurface();
    }

    int faceCount() const
    {
      return this->faceCount_;
    }

    bool canAddFace() const
    {
      return this->faceCount_ < loka::app::RectSurfaceModel::kMaxRects;
    }

    loka::core::State<loka::app::RectSurfaceModel> *surfaceModel()
    {
      return &this->surfaceModel_;
    }

#if defined(TEST_BUILD)
    bool addFaceForTesting(const loka::app::RectSprite &sprite, short velocityX, short velocityY)
    {
      if (this->faceCount_ >= loka::app::RectSurfaceModel::kMaxRects)
      {
        return false;
      }
      loka::core::StateTrackerGuard guard(&this->tracker_);
      Face &face = this->faces_[this->faceCount_++];
      face.sprite = sprite;
      face.velocityX = velocityX;
      face.velocityY = velocityY;
      this->clampFace(face);
      this->publishSurface();
      return true;
    }

    const loka::app::RectSprite &faceForTesting(int index) const
    {
      assert(index >= 0 && index < this->faceCount_);
      return this->faces_[index].sprite;
    }

    short velocityXForTesting(int index) const
    {
      assert(index >= 0 && index < this->faceCount_);
      return this->faces_[index].velocityX;
    }
#endif

  private:
    static short clampWall(int value)
    {
      if (value <= 0)
      {
        return 0;
      }
      return value > SHRT_MAX ? SHRT_MAX : static_cast<short>(value);
    }

    Face spawnFace(short index) const
    {
      SpawnRandom random(static_cast<unsigned long>(index) + 1UL);
      Face face;
      face.sprite.width = kFaceSize;
      face.sprite.height = kFaceSize;
      const int maximumX = this->maximumPosition(this->boundsWidth_);
      const int maximumY = this->maximumPosition(this->boundsHeight_);
      face.sprite.x = static_cast<short>(random.nextIndex(maximumX + 1));
      face.sprite.y = static_cast<short>(random.nextIndex(maximumY + 1));
      const short speedX = static_cast<short>(1 + random.nextIndex(3));
      const short speedY = static_cast<short>(1 + random.nextIndex(3));
      face.velocityX = (random.next() & 1UL) ? speedX : static_cast<short>(-speedX);
      face.velocityY = (random.next() & 1UL) ? speedY : static_cast<short>(-speedY);
      return face;
    }

    static int maximumPosition(short bound)
    {
      const int maximum = static_cast<int>(bound) - kFaceSize;
      return maximum > 0 ? maximum : 0;
    }

    static void reflectAxis(short &position, short &velocity, int maximum)
    {
      if (maximum <= 0)
      {
        position = 0;
        return;
      }
      int next = static_cast<int>(position) + static_cast<int>(velocity);
      while (next < 0 || next > maximum)
      {
        if (next < 0)
        {
          next = -next;
          velocity = static_cast<short>(-velocity);
        }
        if (next > maximum)
        {
          next = maximum - (next - maximum);
          velocity = static_cast<short>(-velocity);
        }
      }
      position = static_cast<short>(next);
    }

    void stepFaces()
    {
      const int maximumX = this->maximumPosition(this->boundsWidth_);
      const int maximumY = this->maximumPosition(this->boundsHeight_);
      for (short i = 0; i < this->faceCount_; ++i)
      {
        Face &face = this->faces_[i];
        this->reflectAxis(face.sprite.x, face.velocityX, maximumX);
        this->reflectAxis(face.sprite.y, face.velocityY, maximumY);
      }
    }

    void clampFace(Face &face)
    {
      const int maximumX = this->maximumPosition(this->boundsWidth_);
      const int maximumY = this->maximumPosition(this->boundsHeight_);
      if (face.sprite.x < 0)
      {
        face.sprite.x = 0;
      }
      else if (face.sprite.x > maximumX)
      {
        face.sprite.x = static_cast<short>(maximumX);
      }
      if (face.sprite.y < 0)
      {
        face.sprite.y = 0;
      }
      else if (face.sprite.y > maximumY)
      {
        face.sprite.y = static_cast<short>(maximumY);
      }
    }

    void publishSurface()
    {
      this->cachedModel_.rectCount = this->faceCount_;
      this->cachedModel_.dirtyRectCount = 0;
      for (short i = 0; i < this->faceCount_; ++i)
      {
        this->cachedModel_.rects[i] = this->faces_[i].sprite;
      }
      this->surfaceModel_.set(this->cachedModel_);
    }

    loka::core::PushStateTracker tracker_;
    loka::core::MutableState<loka::app::RectSurfaceModel> surfaceModel_;
    Face faces_[loka::app::RectSurfaceModel::kMaxRects];
    short faceCount_;
    short boundsWidth_;
    short boundsHeight_;
    double accumulatedSeconds_;
    loka::app::RectSurfaceModel cachedModel_;
  };
} // namespace smirkbench

#endif // LOKA_SMIRK_BENCH_SMIRK_MODEL_HPP
