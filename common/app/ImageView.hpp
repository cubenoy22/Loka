#ifndef LOKA_APP2_IMAGE_VIEW_HPP
#define LOKA_APP2_IMAGE_VIEW_HPP

#include "app/scene/Node.hpp"
#include "core/resource/Image.hpp"
#include "loka/core/State.hpp"
#include <cassert>

namespace loka
{
  namespace app
  {
    struct ImageViewTypeTag
    {
    };

    class ImageViewNode;

    struct ImageViewProps : public scene::NodePropsBase<ImageViewProps>
    {
      typedef ImageViewTypeTag TypeTag;
      typedef ImageViewNode NodeType;

      loka::core::State<loka::core::resource::Image> *image_;
      int width_;
      int height_;

      ImageViewProps() : image_(0), width_(0), height_(0) {}

      ImageViewProps &image(loka::core::State<loka::core::resource::Image> *state)
      {
        this->image_ = state;
        return *this;
      }

      ImageViewProps &size(int width, int height)
      {
        this->width_ = width;
        this->height_ = height;
        return *this;
      }

      void assertInitialized() const
      {
        assert(this->image_);
      }

      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        const ImageViewProps &other = static_cast<const ImageViewProps &>(rhs);
        if (image_ != other.image_)
          return image_ < other.image_;
        if (width_ != other.width_)
          return width_ < other.width_;
        return height_ < other.height_;
      }
    };

    class ImageViewNode : public scene::Node
    {
    public:
      typedef ImageViewTypeTag TypeTag;
      ImageViewProps props;
      ImageViewNode(const ImageViewProps &p) : props(p) {}
      virtual scene::NodeKind kind() const { return scene::NODE_KIND_IMAGE_VIEW; }
      virtual ImageViewNode *asImageViewNode() { return this; }
    };

    struct ImageViewDefinition : public scene::NodeDefinition<ImageViewProps, ImageViewNode>
    {
      ImageViewDefinition() : loka::app::scene::NodeDefinition<ImageViewProps, ImageViewNode>() {}
      ImageViewDefinition(const ImageViewProps &p) : loka::app::scene::NodeDefinition<ImageViewProps, ImageViewNode>(p) {}
      ImageViewDefinition &image(loka::core::State<loka::core::resource::Image> *state)
      {
        this->props.image(state);
        return *this;
      }
      ImageViewDefinition &size(int width, int height)
      {
        this->props.size(width, height);
        return *this;
      }
      using loka::app::scene::NodeDefinition<ImageViewProps, ImageViewNode>::create;
    };

    typedef ImageViewDefinition ImageView;
  } // namespace app
} // namespace loka

#endif // LOKA_APP2_IMAGE_VIEW_HPP
