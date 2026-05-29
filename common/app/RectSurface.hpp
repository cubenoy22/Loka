#ifndef LOKA_APP_RECT_SURFACE_HPP
#define LOKA_APP_RECT_SURFACE_HPP

#include <assert.h>
#include "app/scene/Node.hpp"
#include "core/State.hpp"

namespace loka
{
  namespace app
  {
    struct RectSurfaceTypeTag
    {
    };

    struct RectSprite
    {
      short x;
      short y;
      short width;
      short height;

      RectSprite()
          : x(0),
            y(0),
            width(0),
            height(0)
      {
      }
      RectSprite(short left, short top, short rectWidth, short rectHeight)
          : x(left),
            y(top),
            width(rectWidth),
            height(rectHeight)
      {
      }

      bool operator==(const RectSprite &other) const
      {
        return x == other.x && y == other.y && width == other.width && height == other.height;
      }

      bool operator!=(const RectSprite &other) const
      {
        return !(*this == other);
      }
    };

    struct RectSurfaceModel
    {
      enum
      {
        kMaxRects = 16,
        kMaxDirtyRects = 16
      };

      struct DirtyRect
      {
        short x;
        short y;
        short width;
        short height;

        DirtyRect()
            : x(0),
              y(0),
              width(0),
              height(0)
        {
        }
        DirtyRect(short left, short top, short rectWidth, short rectHeight)
            : x(left),
              y(top),
              width(rectWidth),
              height(rectHeight)
        {
        }

        bool operator==(const DirtyRect &other) const
        {
          return x == other.x && y == other.y && width == other.width && height == other.height;
        }

        bool operator!=(const DirtyRect &other) const
        {
          return !(*this == other);
        }
      };

      short rectCount;
      short dirtyRectCount;
      RectSprite rects[kMaxRects];
      DirtyRect dirtyRects[kMaxDirtyRects];

      RectSurfaceModel()
          : rectCount(0),
            dirtyRectCount(0)
      {
      }

      static short clampRectCount(short count)
      {
        if (count < 0)
        {
          return 0;
        }
        if (count > kMaxRects)
        {
          return kMaxRects;
        }
        return count;
      }

      static short clampDirtyRectCount(short count)
      {
        if (count < 0)
        {
          return 0;
        }
        if (count > kMaxDirtyRects)
        {
          return kMaxDirtyRects;
        }
        return count;
      }

      bool operator==(const RectSurfaceModel &other) const
      {
        assert(rectCount >= 0 && rectCount <= kMaxRects);
        assert(dirtyRectCount >= 0 && dirtyRectCount <= kMaxDirtyRects);
        assert(other.rectCount >= 0 && other.rectCount <= kMaxRects);
        assert(other.dirtyRectCount >= 0 && other.dirtyRectCount <= kMaxDirtyRects);
        if (rectCount != other.rectCount || dirtyRectCount != other.dirtyRectCount)
        {
          return false;
        }
        const short safeRectCount = clampRectCount(rectCount);
        const short safeDirtyRectCount = clampDirtyRectCount(dirtyRectCount);
        for (short i = 0; i < safeRectCount; ++i)
        {
          if (!(rects[i] == other.rects[i]))
          {
            return false;
          }
        }
        for (short i = 0; i < safeDirtyRectCount; ++i)
        {
          if (!(dirtyRects[i] == other.dirtyRects[i]))
          {
            return false;
          }
        }
        return true;
      }

      bool operator!=(const RectSurfaceModel &other) const
      {
        return !(*this == other);
      }

      void setDirtyRect(short x, short y, short width, short height)
      {
        dirtyRectCount = 0;
        addDirtyRect(x, y, width, height);
      }

      void addDirtyRect(short x, short y, short width, short height)
      {
        if (width <= 0 || height <= 0)
        {
          return;
        }
        if (dirtyRectCount >= kMaxDirtyRects)
        {
          return;
        }
        dirtyRects[dirtyRectCount++] = DirtyRect(x, y, width, height);
      }
    };

    class RectSurfaceNode;

    struct RectSurfaceProps : public scene::NodePropsBase<RectSurfaceProps>
    {
      typedef RectSurfaceTypeTag TypeTag;
      typedef RectSurfaceNode NodeType;

      loka::core::State<RectSurfaceModel> *model_;
      short width_;
      short height_;
      bool clearBackground_;
      bool useRegionClip_;

      RectSurfaceProps()
          : model_(0),
            width_(0),
            height_(0),
            clearBackground_(true),
            useRegionClip_(false)
      {
      }

      RectSurfaceProps &model(loka::core::State<RectSurfaceModel> *state)
      {
        this->model_ = state;
        return *this;
      }

      RectSurfaceProps &size(short width, short height)
      {
        this->width_ = width;
        this->height_ = height;
        return *this;
      }

      RectSurfaceProps &clearBackground(bool value)
      {
        this->clearBackground_ = value;
        return *this;
      }

      RectSurfaceProps &useRegionClip(bool value)
      {
        this->useRegionClip_ = value;
        return *this;
      }

      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
        {
          return false;
        }
        const RectSurfaceProps &other = static_cast<const RectSurfaceProps &>(rhs);
        if (model_ != other.model_)
        {
          return model_ < other.model_;
        }
        if (width_ != other.width_)
        {
          return width_ < other.width_;
        }
        if (height_ != other.height_)
        {
          return height_ < other.height_;
        }
        if (clearBackground_ != other.clearBackground_)
        {
          return clearBackground_ < other.clearBackground_;
        }
        return useRegionClip_ < other.useRegionClip_;
      }
    };

    class RectSurfaceNode : public scene::Node
    {
    public:
      typedef RectSurfaceTypeTag TypeTag;
      RectSurfaceProps props;

      RectSurfaceNode(const RectSurfaceProps &propsValue)
          : scene::Node(),
            props(propsValue)
      {
      }

      virtual scene::NodeKind kind() const
      {
        return scene::NODE_KIND_RECT_SURFACE;
      }
      virtual RectSurfaceNode *asRectSurfaceNode()
      {
        return this;
      }

      virtual void declareDirtySources(scene::DirtySourceRegistrar &registrar)
      {
        if (this->props.model_)
        {
          registrar.markDirtyOnChange(this->props.model_, scene::NODE_DIRTY_PROPS);
        }
      }
    };

    struct RectSurfaceDefinition : public scene::NodeDefinition<RectSurfaceProps, RectSurfaceNode>,
                                   public scene::TestIdDslMixin<RectSurfaceDefinition>
    {
      typedef scene::NodeDefinition<RectSurfaceProps, RectSurfaceNode> BaseType;

      RectSurfaceDefinition()
          : BaseType()
      {
      }
      RectSurfaceDefinition(const RectSurfaceProps &propsValue)
          : BaseType(propsValue)
      {
      }
      RectSurfaceDefinition(loka::core::State<RectSurfaceModel> *state)
          : BaseType()
      {
        this->props.model(state);
      }

      RectSurfaceDefinition &model(loka::core::State<RectSurfaceModel> *state)
      {
        this->props.model(state);
        return *this;
      }

      RectSurfaceDefinition &size(short width, short height)
      {
        this->props.size(width, height);
        return *this;
      }

      RectSurfaceDefinition &clearBackground(bool value)
      {
        this->props.clearBackground(value);
        return *this;
      }

      RectSurfaceDefinition &useRegionClip(bool value)
      {
        this->props.useRegionClip(value);
        return *this;
      }
    };

    typedef RectSurfaceDefinition RectSurface;
  } // namespace app
} // namespace loka

#endif
