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
        const ColumnProps *other = dynamic_cast<const ColumnProps *>(&rhs);
        if (!other)
        {
          return false;
        }
        return false;
      }
    };

    class ColumnNode : public core::scene::NestableNode
    {
    public:
      typedef ColumnTypeTag TypeTag;
      ColumnProps props;
      ColumnNode(const ColumnProps &p) : props(p) {}
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
        const RowProps *other = dynamic_cast<const RowProps *>(&rhs);
        if (!other)
        {
          return false;
        }
        return false;
      }
    };

    class RowNode : public core::scene::NestableNode
    {
    public:
      typedef RowTypeTag TypeTag;
      RowProps props;
      RowNode(const RowProps &p) : props(p) {}
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
    };

    typedef ColumnDefinition Column;
    typedef ColumnDefinition VStack;
    typedef RowDefinition Row;
    typedef RowDefinition HStack;
  } // namespace app
} // namespace declara

#endif // LOKA_APP2_ROWCOLUMN_HPP
