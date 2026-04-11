#ifndef LOKA_APP2_ROWCOLUMN_HPP
#define LOKA_APP2_ROWCOLUMN_HPP

#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    enum HorizontalAlignment
    {
      HORIZONTAL_ALIGNMENT_LEADING = 0,
      HORIZONTAL_ALIGNMENT_CENTER = 1,
      HORIZONTAL_ALIGNMENT_TRAILING = 2
    };

    enum VerticalAlignment
    {
      VERTICAL_ALIGNMENT_TOP = 0,
      VERTICAL_ALIGNMENT_CENTER = 1,
      VERTICAL_ALIGNMENT_BOTTOM = 2
    };

    struct ColumnTypeTag
    {
    };

    class ColumnNode;

    struct ColumnProps : public scene::NodePropsBase<ColumnProps>
    {
      typedef ColumnTypeTag TypeTag;
      typedef ColumnNode NodeType;
      bool hasHorizontalAlignment_;
      HorizontalAlignment horizontalAlignment_;
      ColumnProps()
          : hasHorizontalAlignment_(false),
            horizontalAlignment_(HORIZONTAL_ALIGNMENT_LEADING) {}
      ColumnProps &alignHorizontal(HorizontalAlignment value)
      {
        this->hasHorizontalAlignment_ = true;
        this->horizontalAlignment_ = value;
        return *this;
      }
      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        const ColumnProps &other = static_cast<const ColumnProps &>(rhs);
        if (this->hasHorizontalAlignment_ != other.hasHorizontalAlignment_)
          return this->hasHorizontalAlignment_ < other.hasHorizontalAlignment_;
        return this->horizontalAlignment_ < other.horizontalAlignment_;
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

    struct ColumnDefinition : public scene::NodeDefinition<ColumnProps, ColumnNode>, public scene::NestableDefinitionBase, public scene::NestableDslMixin<ColumnDefinition>, public scene::TestIdDslMixin<ColumnDefinition>
    {
      typedef scene::NodeDefinition<ColumnProps, ColumnNode> BaseType;
      using scene::NestableDslMixin<ColumnDefinition>::operator<<;
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
      ColumnDefinition &alignHorizontal(HorizontalAlignment value)
      {
        this->props.alignHorizontal(value);
        return *this;
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
      bool hasVerticalAlignment_;
      VerticalAlignment verticalAlignment_;
      RowProps()
          : hasVerticalAlignment_(false),
            verticalAlignment_(VERTICAL_ALIGNMENT_TOP) {}
      RowProps &alignVertical(VerticalAlignment value)
      {
        this->hasVerticalAlignment_ = true;
        this->verticalAlignment_ = value;
        return *this;
      }
      bool operator<(const scene::PropsBase &rhs) const
      {
        if (rhs.propsTypeId() != propsTypeId())
          return false;
        const RowProps &other = static_cast<const RowProps &>(rhs);
        if (this->hasVerticalAlignment_ != other.hasVerticalAlignment_)
          return this->hasVerticalAlignment_ < other.hasVerticalAlignment_;
        return this->verticalAlignment_ < other.verticalAlignment_;
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

    struct RowDefinition : public scene::NodeDefinition<RowProps, RowNode>, public scene::NestableDefinitionBase, public scene::NestableDslMixin<RowDefinition>, public scene::TestIdDslMixin<RowDefinition>
    {
      typedef scene::NodeDefinition<RowProps, RowNode> BaseType;
      using scene::NestableDslMixin<RowDefinition>::operator<<;
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
      RowDefinition &alignVertical(VerticalAlignment value)
      {
        this->props.alignVertical(value);
        return *this;
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
