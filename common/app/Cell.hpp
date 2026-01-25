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
      loka::core::EmitterState *onClick_;
      CellProps() : text_(0), onClick_(0) {}
      CellProps &text(loka::core::State<loka::core::String> *state)
      {
        text_ = state;
        return *this;
      }
      CellProps &text(const char *value)
      {
        text_ = loka::core::StaticState<loka::core::String>(loka::core::String::Literal(value));
        return *this;
      }
      CellProps &text(const loka::core::String &value)
      {
        text_ = loka::core::StaticState<loka::core::String>(value);
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

    class CellNode : public scene::Node
    {
    public:
      typedef CellTypeTag TypeTag;
      CellProps props;
      CellNode(const CellProps &p) : props(p) {}
      virtual scene::NodeKind kind() const { return scene::NODE_KIND_CELL; }
      virtual CellNode *asCellNode() { return this; }
    };

    struct CellDefinition : public scene::NodeDefinition<CellProps, CellNode>
    {
      CellDefinition() : NodeDefinition() {}
      CellDefinition(const CellProps &p) : NodeDefinition(p) {}
      CellDefinition(const char *text) : NodeDefinition()
      {
        this->props.text_ = loka::core::StaticState<loka::core::String>(loka::core::String::Literal(text));
      }
      CellDefinition(loka::core::State<loka::core::String> *text) : NodeDefinition()
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
        this->props.text_ = loka::core::StaticState<loka::core::String>(loka::core::String::Literal(value));
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
