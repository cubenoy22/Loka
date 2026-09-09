#ifndef LOKA_LAZYLIST_CARD_NODE_HPP
#define LOKA_LAZYLIST_CARD_NODE_HPP

#include "app/scene/node/ComponentNode.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/controls/Button.hpp"
#include "core/String.hpp"

namespace lazylist
{
  class CardNode;

  /** Completed card value: rename history travels with the card on every edit. */
  struct CardProps : loka::app::scene::NodePropsBase<CardProps>
  {
    typedef CardProps TypeTag;
    typedef CardNode NodeType;

    CardProps(const loka::core::String &text = loka::core::String(), short n = 0, short renames = 0)
        : label(text),
          number(n),
          renameCount(renames)
    {
    }
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      if (rhs.propsTypeId() != this->propsTypeId())
        return false;
      const CardProps &other = static_cast<const CardProps &>(rhs);
      if (this->number != other.number)
        return this->number < other.number;
      if (this->renameCount != other.renameCount)
        return this->renameCount < other.renameCount;
      return this->label.compare(other.label) < 0;
    }
    bool operator!=(const CardProps &other) const
    {
      return this->number != other.number || this->renameCount != other.renameCount || !this->label.equals(other.label);
    }

    loka::core::String label;
    short number;
    short renameCount;
  };

#if defined(TEST_BUILD)
  namespace testing
  {
    void cardConstructed(short number);
  }
#endif

  /** Local selection lasts for one visible materialization, including retained Props updates. */
  class CardNode : public loka::app::scene::ComponentNodeWithProps<CardProps>
  {
  public:
    explicit CardNode(const CardProps &p)
        : loka::app::scene::ComponentNodeWithProps<CardProps>(p),
          label_(),
          selected_(),
          toggle_()
    {
      this->declareStates(2).state(this->label_, p.label).state(this->selected_, false);
#if defined(TEST_BUILD)
      testing::cardConstructed(p.number);
#endif
    }

    virtual void composeChildren(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      c.declare(Row() << Text(this->label_.state()).TEST_ID("LazyList.Label")
                      << Button("*", &this->toggle_).TEST_ID("LazyList.Marker"));
    }

  private:
    virtual void declareBindings(loka::app::scene::BindingToken &token)
    {
      token.action(this->toggle_, this, &CardNode::toggle);
      this->refreshLabel();
    }
    void toggle()
    {
      this->selected_.set(!this->selected_.get());
      this->refreshLabel();
    }
    void refreshLabel()
    {
      const loka::core::String label =
          this->selected_.get() ? loka::core::String::Literal("* ") + this->props.label : this->props.label;
      // Equal writes still dirty the tracker; retained Props need no such work.
      if (!this->label_.get().equals(label))
        this->label_.set(label);
    }

    loka::app::scene::NodeState<loka::core::String> label_;
    loka::app::scene::NodeState<bool> selected_;
    loka::core::EmitterState toggle_;
  };
} // namespace lazylist
#endif
