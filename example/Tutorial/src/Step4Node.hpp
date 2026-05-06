#ifndef LOKA_TUTORIAL_STEP4_NODE_HPP
#define LOKA_TUTORIAL_STEP4_NODE_HPP

#include "TutorialShared.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/nodes/Text.hpp"
#include "app/scene/FlowSlot.hpp"
#include "app/scene/NodeState.hpp"
#include "app/scene/composition/StdComposition.hpp"
#include "loka/core/State.hpp"
#include "loka/core/String.hpp"
#include "loka/dsl/StateStream.hpp"

namespace tutorial
{
  class Step4Node : public loka::app::scene::BoundaryNodeFor<Step4Node>
  {
  public:
    typedef loka::app::scene::BoundaryPropsFor<Step4Node> PropsType;

    Step4Node(const PropsType &p)
        : loka::app::scene::BoundaryNodeFor<Step4Node>(p),
          itemCount_(),
          itemSummary_(),
          showSummary_(),
          showItem1_(),
          showItem2_(),
          showItem3_(),
          itemSummaryFlow_(),
          addItemEvent_(),
          toggleSummaryEvent_(),
          initialized_(false),
          item1_(loka::app::Text("Item 1")),
          item2_(loka::app::Text("Item 2")),
          item3_(loka::app::Text("Item 3"))
    {
      this->state(this->itemCount_, 0);
      this->state(this->itemSummary_, loka::core::String::Literal("Items: 0"));
      this->state(this->showSummary_, true);
      this->state(this->showItem1_, false);
      this->state(this->showItem2_, false);
      this->state(this->showItem3_, false);
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      if (this->initialized_)
      {
        return;
      }
      this->bindActionForUi(this->addItemEvent_, &Step4Node::addItem);
      this->bindActionForUi(this->toggleSummaryEvent_, &Step4Node::toggleSummary);
      {
        loka::dsl::StateStream<int> itemCountStream = this->itemCount_.stream();
        this->itemSummaryFlow_ //
            .set(itemCountStream.map(loka::dsl::Const("Items: ") + itemCountStream.slot.value()))
            .bindTo(this->itemSummary_);
      }
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      c.declare(VStack()                                    //
                << TutorialTitle("Step 4")                  //
                << Button("Add item", &this->addItemEvent_) //
                << Button("Show/Hide map", &this->toggleSummaryEvent_)
                << (Show(*this->showSummary_.state())   //
                    << Text(this->itemSummary_.state()) //
                    << (Show(*this->showItem1_.state()) << this->item1_)
                    << (Show(*this->showItem2_.state()) << this->item2_)
                    << (Show(*this->showItem3_.state()) << this->item3_))
                << TutorialHint("StdComposition can still reveal predeclared children while state changes update "
                                "derived display text."));
    }

  private:
    void addItem()
    {
      int next = this->itemCount_.get();
      if (next < 3)
      {
        ++next;
      }
      this->itemCount_.set(next);
      this->showItem1_.set(next >= 1);
      this->showItem2_.set(next >= 2);
      this->showItem3_.set(next >= 3);
    }

    void toggleSummary()
    {
      this->showSummary_.set(!this->showSummary_.get(), true);
    }

    loka::app::scene::NodeState<int> itemCount_;
    loka::app::scene::NodeState<loka::core::String> itemSummary_;
    loka::app::scene::NodeState<bool> showSummary_;
    loka::app::scene::NodeState<bool> showItem1_;
    loka::app::scene::NodeState<bool> showItem2_;
    loka::app::scene::NodeState<bool> showItem3_;
    loka::app::scene::FlowSlot<loka::dsl::StateStream<loka::core::String> > itemSummaryFlow_;
    loka::core::EmitterState addItemEvent_;
    loka::core::EmitterState toggleSummaryEvent_;
    bool initialized_;
    loka::app::TextDefinition item1_;
    loka::app::TextDefinition item2_;
    loka::app::TextDefinition item3_;
  };
} // namespace tutorial

#endif // LOKA_TUTORIAL_STEP4_NODE_HPP
