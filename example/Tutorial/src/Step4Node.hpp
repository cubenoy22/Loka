#ifndef LOKA_TUTORIAL_STEP4_NODE_HPP
#define LOKA_TUTORIAL_STEP4_NODE_HPP

#include "TutorialShared.hpp"
#include "app/Button.hpp"
#include "app/RowColumn.hpp"
#include "app/Show.hpp"
#include "app/Text.hpp"
#include "app/scene/BoundState.hpp"
#include "app/scene/node/StaticComposition.hpp"
#include "loka/core/State.hpp"
#include "loka/core/String.hpp"
#include "loka/dsl/Expr.hpp"
#include "loka/dsl/StateStream.hpp"

namespace tutorial {
  class Step4Node : public loka::app::scene::BoundaryNodeFor<Step4Node> {
  public:
    typedef loka::app::scene::BoundaryPropsFor<Step4Node> PropsType;

    Step4Node(const PropsType &p)
        : loka::app::scene::BoundaryNodeFor<Step4Node>(p), itemCount_(), itemSummary_(), showSummary_(), showItem1_(),
          showItem2_(), showItem3_(), addItemEvent_(), toggleSummaryEvent_(), initialized_(false),
          item1_(loka::app::Text("Item 1")), item2_(loka::app::Text("Item 2")), item3_(loka::app::Text("Item 3")) {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c) {
      if (this->initialized_) {
        return;
      }
      c.declareStates()
          .state(this->itemCount_, 0)
          .state(this->itemSummary_, loka::core::String::Literal("Items: 0"))
          .state(this->showSummary_, true)
          .state(this->showItem1_, false)
          .state(this->showItem2_, false)
          .state(this->showItem3_, false);
      {
        loka::dsl::StateStream<int> s = this->itemCount_.stream();
        s.map(loka::dsl::Const("Items: ") + s.slot.value()).set(this->itemSummary_);
      }
      this->bindForUi(this->addItemEvent_, this, &Step4Node::addItem);
      this->bindForUi(this->toggleSummaryEvent_, this, &Step4Node::toggleSummary);
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c) {
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
                << TutorialHint("Static composition can still reveal predeclared children, and stream().map() "
                                "can derive display text."));
    }

  private:
    void addItem() {
      int next = this->itemCount_.get();
      if (next < 3) {
        ++next;
      }
      this->itemCount_.set(next);
      this->showItem1_.set(next >= 1);
      this->showItem2_.set(next >= 2);
      this->showItem3_.set(next >= 3);
    }

    void toggleSummary() {
      this->showSummary_.set(!this->showSummary_.get(), true);
    }

    loka::app::scene::BoundState<int> itemCount_;
    loka::app::scene::BoundState<loka::core::String> itemSummary_;
    loka::app::scene::BoundState<bool> showSummary_;
    loka::app::scene::BoundState<bool> showItem1_;
    loka::app::scene::BoundState<bool> showItem2_;
    loka::app::scene::BoundState<bool> showItem3_;
    loka::core::EmitterState addItemEvent_;
    loka::core::EmitterState toggleSummaryEvent_;
    bool initialized_;
    loka::app::TextDefinition item1_;
    loka::app::TextDefinition item2_;
    loka::app::TextDefinition item3_;
  };
} // namespace tutorial

#endif // LOKA_TUTORIAL_STEP4_NODE_HPP
