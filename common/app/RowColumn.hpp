#ifndef LOKA_APP2_ROWCOLUMN_HPP
#define LOKA_APP2_ROWCOLUMN_HPP

#include "core2/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    struct ColumnTypeTag
    {
    };

    class ColumnNode;

    struct ColumnProps : public scene::NodePropsBase<ColumnProps>
    {
      typedef ColumnTypeTag TypeTag;
      typedef ColumnNode NodeType;
      ColumnProps() {}
      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        return false; // no fields to compare
      }
    };

    class ColumnNode : public scene::NestableNode
    {
    public:
      typedef ColumnTypeTag TypeTag;
      ColumnProps props;
      ColumnNode(const ColumnProps &p) : props(p) {}
      virtual scene::NodeKind kind() const { return scene::NODE_KIND_COLUMN; }
      virtual ColumnNode *asColumnNode() { return this; }
    };

    struct ColumnDefinition : public scene::NodeDefinition<ColumnProps, ColumnNode>, public scene::NestableDefinitionBase
    {
      typedef scene::NodeDefinition<ColumnProps, ColumnNode> BaseType;
      ColumnDefinition() : BaseType(), scene::NestableDefinitionBase() {}
      ColumnDefinition(const ColumnProps &p) : BaseType(p), scene::NestableDefinitionBase() {}
      ColumnDefinition(const ColumnDefinition &other) : BaseType(other), scene::NestableDefinitionBase(other) {}
      ColumnDefinition &operator=(const ColumnDefinition &other)
      {
        if (this != &other)
        {
          BaseType::operator=(other);
          scene::NestableDefinitionBase::operator=(other);
        }
        return *this;
      }
      virtual scene::NodeDefinitionBase *clone() const
      {
        return new ColumnDefinition(*this);
      }
      virtual scene::INestableDefinition *asNestableDefinition() { return this; }
      virtual const scene::NodeDefinitionBase *asNodeDefinitionBase() const { return this; }
    };

    struct RowTypeTag
    {
    };

    class RowNode;

    struct RowProps : public scene::NodePropsBase<RowProps>
    {
      typedef RowTypeTag TypeTag;
      typedef RowNode NodeType;
      RowProps() {}
      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        return false; // no fields to compare
      }
    };

    class RowNode : public scene::NestableNode
    {
    public:
      typedef RowTypeTag TypeTag;
      RowProps props;
      RowNode(const RowProps &p) : props(p) {}
      virtual scene::NodeKind kind() const { return scene::NODE_KIND_ROW; }
      virtual RowNode *asRowNode() { return this; }
    };

    struct RowDefinition : public scene::NodeDefinition<RowProps, RowNode>, public scene::NestableDefinitionBase
    {
      typedef scene::NodeDefinition<RowProps, RowNode> BaseType;
      RowDefinition() : BaseType(), scene::NestableDefinitionBase() {}
      RowDefinition(const RowProps &p) : BaseType(p), scene::NestableDefinitionBase() {}
      RowDefinition(const RowDefinition &other) : BaseType(other), scene::NestableDefinitionBase(other) {}
      RowDefinition &operator=(const RowDefinition &other)
      {
        if (this != &other)
        {
          BaseType::operator=(other);
          scene::NestableDefinitionBase::operator=(other);
        }
        return *this;
      }
      virtual scene::NodeDefinitionBase *clone() const
      {
        return new RowDefinition(*this);
      }
      virtual scene::INestableDefinition *asNestableDefinition() { return this; }
      virtual const scene::NodeDefinitionBase *asNodeDefinitionBase() const { return this; }
    };

    typedef ColumnDefinition Column;
    typedef ColumnDefinition VStack;
    typedef RowDefinition Row;
    typedef RowDefinition HStack;
  } // namespace app
} // namespace loka

#endif // LOKA_APP2_ROWCOLUMN_HPP
