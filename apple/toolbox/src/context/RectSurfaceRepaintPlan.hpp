#ifndef LOKA_TOOLBOX_RECT_SURFACE_REPAINT_PLAN_HPP
#define LOKA_TOOLBOX_RECT_SURFACE_REPAINT_PLAN_HPP

#include "app/RectSurface.hpp"
#include "core/Frame.hpp"

namespace loka
{
  namespace toolbox
  {
    /** Completed, allocation-free repaint commands in surface placement coordinates.
        Execute all erases before all paints. Index is sprite identity: a
        retained sprite erases previous(i) minus current(i) and paints
        current(i) minus previous(i); the part of current(i) it already
        painted last frame is repainted only when some erase in this plan
        crosses it (another sprite's trailing strip). A null previous model
        clears the surface; clearBackground=false never erases. The caller
        retains responsibility for the native region clip. */
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
        const short previousCount =
            previous ? app::RectSurfaceModel::clampRectCount(previous->rectCount) : static_cast<short>(0);
        for (short i = 0; i < currentCount; ++i)
        {
          const core::Frame newRect = placed(current.rects[i], surface);
          if (i < previousCount)
          {
            const core::Frame oldRect = placed(previous->rects[i], surface);
            const core::Frame kept = intersection(oldRect, newRect);
            if (kept.hasSize() && oldRect.width == newRect.width && oldRect.height == newRect.height)
            {
              // Strips of current(i) outside the kept overlap.
              this->appendPaint(core::Frame(newRect.x, newRect.y, newRect.width, kept.y - newRect.y), dirty);
              this->appendPaint(core::Frame(newRect.x,
                                            kept.y + kept.height,
                                            newRect.width,
                                            newRect.y + newRect.height - kept.y - kept.height),
                                dirty);
              this->appendPaint(core::Frame(newRect.x, kept.y, kept.x - newRect.x, kept.height), dirty);
              this->appendPaint(core::Frame(kept.x + kept.width,
                                            kept.y,
                                            newRect.x + newRect.width - kept.x - kept.width,
                                            kept.height),
                                dirty);
              if (this->anyEraseCrosses(kept))
              {
                this->appendPaint(kept, dirty);
              }
              continue;
            }
          }
          this->appendPaint(newRect, dirty);
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

      bool anyEraseCrosses(const core::Frame &rect) const
      {
        for (short i = 0; i < this->eraseCount_; ++i)
        {
          if (intersection(this->eraseRects_[i], rect).hasSize())
          {
            return true;
          }
        }
        return false;
      }

      void appendPaint(const core::Frame &rect, const core::Frame &dirty)
      {
        const core::Frame clipped = intersection(rect, dirty);
        if (clipped.hasSize())
        {
          // At most four strips plus the kept overlap per current index.
          this->paintRects_[this->paintCount_++] = clipped;
        }
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
      core::Frame paintRects_[kMaxSprites * 5];
      short eraseCount_;
      short paintCount_;
    };
  } // namespace toolbox
} // namespace loka

#endif
