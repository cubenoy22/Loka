#ifndef LOKA_TOOLBOX_RECT_SURFACE_REPAINT_PLAN_HPP
#define LOKA_TOOLBOX_RECT_SURFACE_REPAINT_PLAN_HPP

#include "app/RectSurface.hpp"
#include "core/Frame.hpp"

namespace loka
{
  namespace toolbox
  {
    /** Completed, allocation-free repaint commands in surface placement coordinates.
        Execute all erases before all paints: another sprite's erase can cross
        an unchanged portion of a current sprite. Index is sprite identity.
        A null previous model clears the surface; clearBackground=false never
        erases. The caller retains responsibility for the native region clip. */
    class RectSurfaceRepaintPlan
    {
    public:
      enum
      {
        kMaxSprites = app::RectSurfaceModel::kMaxRects
      };

      RectSurfaceRepaintPlan(const app::RectSurfaceModel *previous,
                             const app::RectSurfaceModel &current,
                             const core::Frame &surface,
                             const core::Frame &dirty,
                             bool clearBackground)
          : eraseCount_(0),
            paintCount_(0)
      {
        const short currentCount = app::RectSurfaceModel::clampRectCount(current.rectCount);
        if (clearBackground)
        {
          if (!previous)
          {
            this->appendErase(surface, dirty);
          }
          else
          {
            const short previousCount = app::RectSurfaceModel::clampRectCount(previous->rectCount);
            for (short i = 0; i < previousCount; ++i)
            {
              const core::Frame oldRect = placed(previous->rects[i], surface);
              if (i >= currentCount)
              {
                this->appendErase(oldRect, dirty);
                continue;
              }
              const core::Frame overlap = intersection(oldRect, placed(current.rects[i], surface));
              if (!overlap.hasSize())
              {
                this->appendErase(oldRect, dirty);
                continue;
              }
              this->appendErase(core::Frame(oldRect.x, oldRect.y, oldRect.width, overlap.y - oldRect.y), dirty);
              this->appendErase(core::Frame(oldRect.x,
                                            overlap.y + overlap.height,
                                            oldRect.width,
                                            oldRect.y + oldRect.height - overlap.y - overlap.height),
                                dirty);
              this->appendErase(core::Frame(oldRect.x, overlap.y, overlap.x - oldRect.x, overlap.height), dirty);
              this->appendErase(core::Frame(overlap.x + overlap.width,
                                            overlap.y,
                                            oldRect.x + oldRect.width - overlap.x - overlap.width,
                                            overlap.height),
                                dirty);
            }
          }
        }
        for (short i = 0; i < currentCount; ++i)
        {
          const core::Frame rect = intersection(placed(current.rects[i], surface), dirty);
          if (rect.hasSize())
          {
            this->paintRects_[this->paintCount_++] = rect;
          }
        }
      }

      short eraseCount() const
      {
        return this->eraseCount_;
      }
      short paintCount() const
      {
        return this->paintCount_;
      }
      const core::Frame &eraseRect(short index) const
      {
        assert(index >= 0 && index < this->eraseCount_);
        return this->eraseRects_[index];
      }
      const core::Frame &paintRect(short index) const
      {
        assert(index >= 0 && index < this->paintCount_);
        return this->paintRects_[index];
      }

    private:
      static core::Frame placed(const app::RectSprite &sprite, const core::Frame &surface)
      {
        return core::Frame(surface.x + sprite.x, surface.y + sprite.y, sprite.width, sprite.height);
      }

      static core::Frame intersection(const core::Frame &a, const core::Frame &b)
      {
        const int left = a.x > b.x ? a.x : b.x;
        const int top = a.y > b.y ? a.y : b.y;
        const int right = a.x + a.width < b.x + b.width ? a.x + a.width : b.x + b.width;
        const int bottom = a.y + a.height < b.y + b.height ? a.y + a.height : b.y + b.height;
        return core::Frame(left, top, right - left, bottom - top);
      }

      void appendErase(const core::Frame &rect, const core::Frame &dirty)
      {
        const core::Frame clipped = intersection(rect, dirty);
        if (clipped.hasSize())
        {
          // Clamped model counts and at most four strips per index bound this
          // write in release builds too; no assertion is a storage wall.
          this->eraseRects_[this->eraseCount_++] = clipped;
        }
      }

      core::Frame eraseRects_[kMaxSprites * 4];
      core::Frame paintRects_[kMaxSprites];
      short eraseCount_;
      short paintCount_;
    };
  } // namespace toolbox
} // namespace loka

#endif
