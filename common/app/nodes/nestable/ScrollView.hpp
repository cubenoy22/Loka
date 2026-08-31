#ifndef LOKA_APP_NODES_NESTABLE_SCROLL_VIEW_HPP
#define LOKA_APP_NODES_NESTABLE_SCROLL_VIEW_HPP

#include "app/scene/Node.hpp"
#include "app/scene/state/NodeState.hpp"
#include "core/State.hpp"

namespace loka
{
  namespace app
  {
    struct ScrollViewTypeTag
    {
    };

    class ScrollViewNode;

    /** Stable inputs for a scrollable projection-parent scope. offset_ is the
        rail-published fact; scrollTo_ is application intent and is consumed
        by platform rails when their scrolling implementation is installed. */
    struct ScrollViewProps : public scene::NodePropsBase<ScrollViewProps>
    {
      typedef ScrollViewTypeTag TypeTag;
      typedef ScrollViewNode NodeType;

      loka::core::MutableState<int> *offset_;
      loka::core::MutableState<int> *scrollTo_;

      ScrollViewProps()
          : offset_(0),
            scrollTo_(0)
      {
      }

      explicit ScrollViewProps(const scene::NodeState<int> &offset)
          : offset_(offset.dangerouslyMutableState()),
            scrollTo_(0)
      {
      }

      /** Binds the rail-published scroll offset fact. */
      ScrollViewProps &offset(const scene::NodeState<int> &value)
      {
        this->offset_ = value.dangerouslyMutableState();
        return *this;
      }

      /** Binds application-authored scroll intent. Projection is installed by
          the platform rail implementations, not by this contract header. */
      ScrollViewProps &scrollTo(loka::core::MutableState<int> *value)
      {
        this->scrollTo_ = value;
        return *this;
      }

      ScrollViewProps &scrollTo(const scene::NodeState<int> &value)
      {
        this->scrollTo_ = value.dangerouslyMutableState();
        return *this;
      }

      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != this->propsTypeId())
        {
          return false;
        }
        const ScrollViewProps &other = static_cast<const ScrollViewProps &>(rhs);
        if (this->offset_ != other.offset_)
        {
          return this->offset_ < other.offset_;
        }
        return this->scrollTo_ < other.scrollTo_;
      }
    };

    class ScrollViewNode : public scene::NestableNode
    {
    public:
      typedef ScrollViewTypeTag TypeTag;

      explicit ScrollViewNode(const ScrollViewProps &p)
          : scene::NestableNode(),
            props(p)
      {
      }

      virtual scene::NodeKind kind() const
      {
        return scene::NODE_KIND_SCROLL_VIEW;
      }

      virtual const void *nodeTypeKey() const
      {
        return scene::NodeTypeToken<ScrollViewNode>();
      }

      virtual ScrollViewNode *asScrollViewNode()
      {
        return this;
      }

      virtual void declareDirtySources(scene::DirtySourceRegistrar &registrar)
      {
        if (this->props.offset_)
        {
          registrar.markDirtyOnChange(this->props.offset_, scene::NODE_DIRTY_LAYOUT);
        }
      }

      ScrollViewProps props;
    };

    struct ScrollViewDefinition
        : public scene::NestableNodeDefinition<ScrollViewProps, ScrollViewNode, ScrollViewDefinition>,
          public scene::TestIdDslMixin<ScrollViewDefinition>
    {
      typedef scene::NestableNodeDefinition<ScrollViewProps, ScrollViewNode, ScrollViewDefinition> BaseType;
      using BaseType::operator<<;

      ScrollViewDefinition()
          : BaseType()
      {
      }

      explicit ScrollViewDefinition(const ScrollViewProps &p)
          : BaseType(p)
      {
      }

      explicit ScrollViewDefinition(const scene::NodeState<int> &offset)
          : BaseType(ScrollViewProps(offset))
      {
      }

      ScrollViewDefinition(const ScrollViewDefinition &other)
          : BaseType(other)
      {
      }

      ScrollViewDefinition &offset(const scene::NodeState<int> &value)
      {
        this->props.offset(value);
        return *this;
      }

      ScrollViewDefinition &scrollTo(loka::core::MutableState<int> *value)
      {
        this->props.scrollTo(value);
        return *this;
      }

      ScrollViewDefinition &scrollTo(const scene::NodeState<int> &value)
      {
        this->props.scrollTo(value);
        return *this;
      }
    };

    typedef ScrollViewDefinition ScrollView;

    inline ScrollViewNode *createNode(const ScrollViewProps &props)
    {
      return new ScrollViewNode(props);
    }
  } // namespace app
} // namespace loka

#endif // LOKA_APP_NODES_NESTABLE_SCROLL_VIEW_HPP
