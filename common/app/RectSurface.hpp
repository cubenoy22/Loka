#ifndef LOKA_APP_RECT_SURFACE_HPP
#define LOKA_APP_RECT_SURFACE_HPP

#include <assert.h>
#include <limits.h>
#include "app/scene/Node.hpp"
#include "core/State.hpp"
#include "core/resource/Image.hpp"

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

    /** An image at intrinsic size. Alpha rails derive its per-pixel mask from
        an alpha channel and otherwise paint it opaque; Classic uses the PICT
        white-key mask. */
    struct ImageSprite
    {
      short x;
      short y;
      loka::core::resource::Image image;

      ImageSprite(short left, short top, const loka::core::resource::Image &imageValue)
          : x(left),
            y(top),
            image(imageValue)
      {
      }
    };

    /** One RectSurface entry. Geometry is common to both kinds. The image
        payload is meaningful only for KIND_IMAGE and is exposed through the
        kind-refusing query rather than as a generally readable field. */
    class RectSurfaceSprite
    {
    public:
      enum Kind
      {
        KIND_RECT = 0,
        KIND_IMAGE
      };

      short x;
      short y;
      short width;
      short height;

      RectSurfaceSprite()
          : x(0),
            y(0),
            width(0),
            height(0),
            kind_(KIND_RECT),
            image_()
      {
      }

      RectSurfaceSprite(const RectSprite &rect)
          : x(rect.x),
            y(rect.y),
            width(rect.width),
            height(rect.height),
            kind_(KIND_RECT),
            image_()
      {
      }

      RectSurfaceSprite(const ImageSprite &sprite)
          : x(sprite.x),
            y(sprite.y),
            width(static_cast<short>(sprite.image.width())),
            height(static_cast<short>(sprite.image.height())),
            kind_(KIND_IMAGE),
            image_(sprite.image)
      {
      }

      Kind kind() const
      {
        return kind_;
      }

      bool queryImage(loka::core::resource::Image &out) const
      {
        if (kind_ != KIND_IMAGE)
        {
          return false;
        }
        out = image_;
        return true;
      }

      bool operator==(const RectSurfaceSprite &other) const
      {
        if (kind_ != other.kind_ || x != other.x || y != other.y || width != other.width || height != other.height)
        {
          return false;
        }
        return kind_ != KIND_IMAGE || image_ == other.image_;
      }

      bool operator!=(const RectSurfaceSprite &other) const
      {
        return !(*this == other);
      }

    private:
      Kind kind_;
      loka::core::resource::Image image_;
    };

    /** Returns whether publishing current in place of previous can change the
        pixels at their shared array position. IMAGE identity is content for
        this decision even when its intrinsic geometry is unchanged. */
    inline bool RectSurfaceSpriteRequiresRepaint(const RectSurfaceSprite &current,
                                                 const RectSurfaceSprite &previous)
    {
      return current != previous;
    }

    struct RectSurfaceModel
    {
      enum
      {
        // A fixed ABI constant: the model's size and every rail core's
        // per-sprite arrays are compiled from it, so it is not a per-target
        // knob (a mismatch between an app and its rail core would index past
        // the smaller array).
        kMaxSprites = 16,
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

    private:
      short spriteCount_;
      RectSurfaceSprite sprites_[kMaxSprites];

    public:
      short dirtyRectCount;
      DirtyRect dirtyRects[kMaxDirtyRects];

      RectSurfaceModel()
          : spriteCount_(0),
            dirtyRectCount(0)
      {
      }

      short spriteCount() const
      {
        return spriteCount_;
      }

      /** Returns one committed sprite without exposing mutable model storage. */
      const RectSurfaceSprite &sprite(short index) const
      {
        assert(index >= 0 && index < spriteCount());
        return sprites_[index];
      }

      /** Empties the active prefix and releases payloads before publishing the
          new count. Inactive slots therefore never retain an Image. */
      void clear()
      {
        for (short i = 0; i < spriteCount_; ++i)
        {
          sprites_[i] = RectSurfaceSprite();
        }
        spriteCount_ = 0;
      }

      bool add(const RectSprite &sprite)
      {
        return addSprite(RectSurfaceSprite(sprite));
      }

      bool add(const ImageSprite &sprite)
      {
        const int width = sprite.image.width();
        const int height = sprite.image.height();
        if (width < 1 || width > SHRT_MAX || height < 1 || height > SHRT_MAX)
        {
          return false;
        }
        return addSprite(RectSurfaceSprite(sprite));
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
        assert(spriteCount_ >= 0 && spriteCount_ <= kMaxSprites);
        assert(dirtyRectCount >= 0 && dirtyRectCount <= kMaxDirtyRects);
        assert(other.spriteCount_ >= 0 && other.spriteCount_ <= kMaxSprites);
        assert(other.dirtyRectCount >= 0 && other.dirtyRectCount <= kMaxDirtyRects);
        if (spriteCount_ != other.spriteCount_ || dirtyRectCount != other.dirtyRectCount)
        {
          return false;
        }
        const short safeDirtyRectCount = clampDirtyRectCount(dirtyRectCount);
        for (short i = 0; i < spriteCount_; ++i)
        {
          if (!(sprite(i) == other.sprite(i)))
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

    private:
      bool addSprite(const RectSurfaceSprite &sprite)
      {
        if (spriteCount_ >= kMaxSprites)
        {
          return false;
        }
        sprites_[spriteCount_++] = sprite;
        return true;
      }
    };

    enum RectSurfacePaintResult
    {
      RECT_SURFACE_PAINT_SUCCEEDED = 0,
      RECT_SURFACE_PAINT_REFUSED
    };

    /** Commits a model only after its pixels were applied. Refusal preserves
        the caller-owned applied snapshot for a later retry. */
    inline void FinishRectSurfacePaint(RectSurfacePaintResult result,
                                       const RectSurfaceModel &requested,
                                       RectSurfaceModel &applied)
    {
      switch (result)
      {
      case RECT_SURFACE_PAINT_SUCCEEDED:
        applied = requested;
        break;
      case RECT_SURFACE_PAINT_REFUSED:
        break;
      }
    }

    /** Read-only array-order view used by native RectSurface paint passes. */
    class RectSurfacePaintList
    {
    public:
      explicit RectSurfacePaintList(const RectSurfaceModel &model)
          : model_(model)
      {
      }

      short count() const
      {
        return model_.spriteCount();
      }

      const RectSurfaceSprite *querySprite(short index) const
      {
        if (index < 0 || index >= count())
        {
          return 0;
        }
        return &model_.sprite(index);
      }

    private:
      const RectSurfaceModel &model_;
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
