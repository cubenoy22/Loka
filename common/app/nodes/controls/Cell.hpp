#ifndef LOKA_APP2_CELL_HPP
#define LOKA_APP2_CELL_HPP

#include "loka/core/State.hpp"
#include "loka/core/String.hpp"
#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    struct CellTypeTag
    {
    };

    class CellNode;

    struct CellProps : public scene::NodePropsBase<CellProps>
    {
      typedef CellTypeTag TypeTag;
      typedef CellNode NodeType;
      loka::core::State<loka::core::String> *text_;
      loka::core::MutableState<loka::core::String> ownedText_;
      bool ownsText_;
      loka::core::EmitterState *onClick_;
      CellProps() : text_(0), ownedText_(), ownsText_(false), onClick_(0) {}
      CellProps(const CellProps &other)
          : text_(other.text_), ownedText_(other.ownedText_), ownsText_(other.ownsText_), onClick_(other.onClick_)
      {
        if (ownsText_)
        {
          text_ = &ownedText_;
        }
      }
      CellProps &operator=(const CellProps &other)
      {
        if (this != &other)
        {
          text_ = other.text_;
          ownedText_ = other.ownedText_;
          ownsText_ = other.ownsText_;
          onClick_ = other.onClick_;
          if (ownsText_)
          {
            text_ = &ownedText_;
          }
        }
        return *this;
      }
      CellProps &text(loka::core::State<loka::core::String> *state)
      {
        text_ = state;
        ownsText_ = false;
        return *this;
      }
      CellProps &text(const char *value)
      {
        return text(loka::core::String::Literal(value));
      }
      CellProps &text(const loka::core::String &value)
      {
        ownedText_ = loka::core::MutableState<loka::core::String>(value);
        text_ = &ownedText_;
        ownsText_ = true;
        return *this;
      }
      CellProps &onClick(loka::core::EmitterState *e)
      {
        onClick_ = e;
        return *this;
      }
      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        const CellProps &other = static_cast<const CellProps &>(rhs);
        if (text_ != other.text_)
        {
          return text_ < other.text_;
        }
        return onClick_ < other.onClick_;
      }
    };

    class CellNode : public scene::Node,
                     public scene::IProjectedLayoutNode
    {
    public:
      typedef CellTypeTag TypeTag;
      CellProps props;
      CellNode(const CellProps &p) : props(p) {}
      virtual scene::NodeKind kind() const { return scene::NODE_KIND_CELL; }
      virtual scene::IProjectedLayoutNode *asProjectedLayoutNode() { return this; }
      virtual const void *nodeTypeKey() const { return scene::NodeTypeToken<CellNode>(); }
      virtual CellNode *asCellNode() { return this; }
      virtual short layoutProjected(scene::IPlatformController *controller, scene::LayoutState &state)
      {
        if (!controller)
        {
          return state.y;
        }
        if (!scene::PrepareProjectedLayout(controller, this, state))
        {
          return state.y;
        }
        return scene::Node::layout(controller, state);
      }
      virtual void declareDirtySources(scene::DirtySourceRegistrar &registrar)
      {
        if (this->props.text_ && !this->props.ownsText_)
        {
          registrar.markDirtyOnChange(this->props.text_, scene::NODE_DIRTY_PROPS);
        }
      }
    };

    struct CellDefinition : public scene::NodeDefinition<CellProps, CellNode>, public scene::TestIdDslMixin<CellDefinition>
    {
      CellDefinition() : loka::app::scene::NodeDefinition<CellProps, CellNode>() {}
      CellDefinition(const CellProps &p) : loka::app::scene::NodeDefinition<CellProps, CellNode>(p) {}
      CellDefinition(const char *text) : loka::app::scene::NodeDefinition<CellProps, CellNode>()
      {
        this->props.text(text);
      }
      CellDefinition(loka::core::State<loka::core::String> *text) : loka::app::scene::NodeDefinition<CellProps, CellNode>()
      {
        this->props.text_ = text;
      }
      CellDefinition &text(loka::core::State<loka::core::String> *state)
      {
        this->props.text_ = state;
        return *this;
      }
      CellDefinition &text(const char *value)
      {
        this->props.text(value);
        return *this;
      }
      CellDefinition &onClick(loka::core::EmitterState *e)
      {
        this->props.onClick_ = e;
        return *this;
      }
    };

    typedef CellDefinition Cell;
  } // namespace app
} // namespace loka

#endif // LOKA_APP2_CELL_HPP
