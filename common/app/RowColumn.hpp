#ifndef LOKA_APP2_ROWCOLUMN_HPP
#define LOKA_APP2_ROWCOLUMN_HPP

#include "core2/scene/Node.hpp"

namespace declara
{
  namespace app
  {
    struct ColumnTypeTag
    {
    };

    class ColumnNode;

    struct ColumnProps : public core::scene::NodePropsBase<ColumnProps>
    {
      typedef ColumnTypeTag TypeTag;
      typedef ColumnNode NodeType;
      ColumnProps() {}
      bool operator<(const core::scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        return false; // no fields to compare
      }
    };

    class ColumnNode : public core::scene::NestableNode
    {
    public:
      typedef ColumnTypeTag TypeTag;
      ColumnProps props;
      ColumnNode(const ColumnProps &p) : props(p) {}
      virtual core::scene::NodeKind kind() const { return core::scene::NODE_KIND_COLUMN; }
      virtual ColumnNode *asColumnNode() { return this; }
    };

    struct ColumnDefinition : public core::scene::NodeDefinition<ColumnProps, ColumnNode>, public core::scene::NestableDefinitionBase
    {
      typedef core::scene::NodeDefinition<ColumnProps, ColumnNode> BaseType;
      ColumnDefinition() : BaseType(), core::scene::NestableDefinitionBase() {}
      ColumnDefinition(const ColumnProps &p) : BaseType(p), core::scene::NestableDefinitionBase() {}
      ColumnDefinition(const ColumnDefinition &other) : BaseType(other), core::scene::NestableDefinitionBase(other) {}
      ColumnDefinition &operator=(const ColumnDefinition &other)
      {
        if (this != &other)
        {
          BaseType::operator=(other);
          core::scene::NestableDefinitionBase::operator=(other);
        }
        return *this;
      }
      virtual core::scene::NodeDefinitionBase *clone() const
      {
        return new ColumnDefinition(*this);
      }
      virtual core::scene::INestableDefinition *asNestableDefinition() { return this; }
      virtual const core::scene::NodeDefinitionBase *asNodeDefinitionBase() const { return this; }
    };

    struct RowTypeTag
    {
    };

    class RowNode;

    struct RowProps : public core::scene::NodePropsBase<RowProps>
    {
      typedef RowTypeTag TypeTag;
      typedef RowNode NodeType;
      RowProps() {}
      bool operator<(const core::scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        return false; // no fields to compare
      }
    };

    class RowNode : public core::scene::NestableNode
    {
    public:
      typedef RowTypeTag TypeTag;
      RowProps props;
      RowNode(const RowProps &p) : props(p) {}
      virtual core::scene::NodeKind kind() const { return core::scene::NODE_KIND_ROW; }
      virtual RowNode *asRowNode() { return this; }
    };

    struct RowDefinition : public core::scene::NodeDefinition<RowProps, RowNode>, public core::scene::NestableDefinitionBase
    {
      typedef core::scene::NodeDefinition<RowProps, RowNode> BaseType;
      RowDefinition() : BaseType(), core::scene::NestableDefinitionBase() {}
      RowDefinition(const RowProps &p) : BaseType(p), core::scene::NestableDefinitionBase() {}
      RowDefinition(const RowDefinition &other) : BaseType(other), core::scene::NestableDefinitionBase(other) {}
      RowDefinition &operator=(const RowDefinition &other)
      {
        if (this != &other)
        {
          BaseType::operator=(other);
          core::scene::NestableDefinitionBase::operator=(other);
        }
        return *this;
      }
      virtual core::scene::NodeDefinitionBase *clone() const
      {
        return new RowDefinition(*this);
      }
      virtual core::scene::INestableDefinition *asNestableDefinition() { return this; }
      virtual const core::scene::NodeDefinitionBase *asNodeDefinitionBase() const { return this; }
    };

    typedef ColumnDefinition Column;
    typedef ColumnDefinition VStack;
    typedef RowDefinition Row;
    typedef RowDefinition HStack;
  } // namespace app
} // namespace declara

#endif // LOKA_APP2_ROWCOLUMN_HPP
