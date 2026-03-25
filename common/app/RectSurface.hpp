#ifndef LOKA_APP_RECT_SURFACE_HPP
#define LOKA_APP_RECT_SURFACE_HPP

#include "app/scene/Node.hpp"
#include "loka/core/State.hpp"

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

      RectSprite() : x(0), y(0), width(0), height(0) {}
      RectSprite(short left, short top, short rectWidth, short rectHeight)
          : x(left), y(top), width(rectWidth), height(rectHeight) {}

      bool operator==(const RectSprite &other) const
      {
        return x == other.x &&
               y == other.y &&
               width == other.width &&
               height == other.height;
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
        kMaxRects = 16
      };

      short rectCount;
      bool hasDirtyRect;
      short dirtyX;
      short dirtyY;
      short dirtyWidth;
      short dirtyHeight;
      RectSprite rects[kMaxRects];

      RectSurfaceModel()
          : rectCount(0),
            hasDirtyRect(false),
            dirtyX(0),
            dirtyY(0),
            dirtyWidth(0),
            dirtyHeight(0)
      {
      }

      bool operator==(const RectSurfaceModel &other) const
      {
        if (rectCount != other.rectCount ||
            hasDirtyRect != other.hasDirtyRect ||
            dirtyX != other.dirtyX ||
            dirtyY != other.dirtyY ||
            dirtyWidth != other.dirtyWidth ||
            dirtyHeight != other.dirtyHeight)
        {
          return false;
        }
        for (short i = 0; i < rectCount; ++i)
        {
          if (!(rects[i] == other.rects[i]))
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
        if (width <= 0 || height <= 0)
        {
          hasDirtyRect = false;
          dirtyX = 0;
          dirtyY = 0;
          dirtyWidth = 0;
          dirtyHeight = 0;
          return;
        }
        hasDirtyRect = true;
        dirtyX = x;
        dirtyY = y;
        dirtyWidth = width;
        dirtyHeight = height;
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

      RectSurfaceProps()
          : model_(0),
            width_(0),
            height_(0),
            clearBackground_(true)
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
        return clearBackground_ < other.clearBackground_;
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

      virtual scene::NodeKind kind() const { return scene::NODE_KIND_RECT_SURFACE; }
      virtual RectSurfaceNode *asRectSurfaceNode() { return this; }

      virtual void declareObservedStates(scene::ObservedStateRegistrar &registrar)
      {
        if (this->props.model_)
        {
          registrar.observe(this->props.model_, scene::NODE_DIRTY_PROPS);
        }
      }
    };

    struct RectSurfaceDefinition : public scene::NodeDefinition<RectSurfaceProps, RectSurfaceNode>,
                                   public scene::TestIdDslMixin<RectSurfaceDefinition>
    {
      typedef scene::NodeDefinition<RectSurfaceProps, RectSurfaceNode> BaseType;

      RectSurfaceDefinition() : BaseType() {}
      RectSurfaceDefinition(const RectSurfaceProps &propsValue) : BaseType(propsValue) {}
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
    };

    typedef RectSurfaceDefinition RectSurface;
  }
}

#endif
