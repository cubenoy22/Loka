#ifndef LOKA_APP2_GRID_HPP
#define LOKA_APP2_GRID_HPP

#include "core2/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    struct GridTypeTag
    {
    };

    class GridNode;

    struct GridProps : public core::scene::NodePropsBase<GridProps>
    {
      typedef GridTypeTag TypeTag;
      typedef GridNode NodeType;
      short rows;
      short cols;
      GridProps() : rows(1), cols(1) {}
      GridProps(short r, short c) : rows(r), cols(c) {}
      bool operator<(const core::scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        const GridProps &other = static_cast<const GridProps &>(rhs);
        if (rows != other.rows)
        {
          return rows < other.rows;
        }
        return cols < other.cols;
      }
    };

    class GridNode : public core::scene::NestableNode
    {
    public:
      typedef GridTypeTag TypeTag;
      GridProps props;
      GridNode(const GridProps &p) : core::scene::NestableNode(), props(p) {}
      virtual core::scene::NodeKind kind() const { return core::scene::NODE_KIND_GRID; }
      virtual GridNode *asGridNode() { return this; }
    };

    struct GridDefinition : public core::scene::NodeDefinition<GridProps, GridNode>, public core::scene::NestableDefinitionBase
    {
      typedef core::scene::NodeDefinition<GridProps, GridNode> BaseType;
      GridDefinition() : BaseType(), NestableDefinitionBase() {}
      GridDefinition(const GridProps &p) : BaseType(p), NestableDefinitionBase() {}
      GridDefinition(const GridDefinition &other) : BaseType(other), NestableDefinitionBase(other) {}
      GridDefinition &operator=(const GridDefinition &other)
      {
        if (this != &other)
        {
          BaseType::operator=(other);
          core::scene::NestableDefinitionBase::operator=(other);
        }
        return *this;
      }
      virtual core::scene::NodeDefinitionBase *clone() const { return new GridDefinition(*this); }
      virtual core::scene::INestableDefinition *asNestableDefinition() { return this; }
      virtual const core::scene::NodeDefinitionBase *asNodeDefinitionBase() const { return this; }
      GridDefinition &rows(short value)
      {
        this->props.rows = value;
        return *this;
      }
      GridDefinition &cols(short value)
      {
        this->props.cols = value;
        return *this;
      }
    };

    typedef GridDefinition Grid;
  } // namespace app
} // namespace loka

#endif // LOKA_APP2_GRID_HPP
