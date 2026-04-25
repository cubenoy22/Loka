#ifndef LOKA_TUTORIAL_STEP2_NODE_HPP
#define LOKA_TUTORIAL_STEP2_NODE_HPP

#include "TutorialShared.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/Text.hpp"
#include "app/scene/BoundState.hpp"
#include "app/scene/nodes/boundary/StdComposition.hpp"
#include "loka/core/State.hpp"
#include "loka/core/String.hpp"

namespace tutorial {
  class Step2Node : public loka::app::scene::BoundaryNodeFor<Step2Node> {
  public:
    typedef loka::app::scene::BoundaryPropsFor<Step2Node> PropsType;

    Step2Node(const PropsType &p)
        : loka::app::scene::BoundaryNodeFor<Step2Node>(p), count_(), countText_(), incrementEvent_(),
          initialized_(false) {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c) {
      if (this->initialized_) {
        return;
      }
      c.declareStates()           //
          .state(this->count_, 0) //
          .state(this->countText_, loka::core::String::Literal("Count: 0"));
      this->bindActionForUi(this->incrementEvent_, &Step2Node::increment);
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c) {
      using namespace loka::app;
      c.declare(VStack()                          //
                << TutorialTitle("Step 2")        //
                << Text(this->countText_.state()) //
                << Button("Increment", &this->incrementEvent_)
                << TutorialHint("A button can update state and reflected text."));
    }

  private:
    void increment() {
      const int next = this->count_.get() + 1;
      this->count_.set(next);
      using loka::core::String;
      this->countText_.set(String::Literal("Count: ") + String::FromInt(next));
    }

    loka::app::scene::BoundState<int> count_;
    loka::app::scene::BoundState<loka::core::String> countText_;
    loka::core::EmitterState incrementEvent_;
    bool initialized_;
  };
} // namespace tutorial

#endif // LOKA_TUTORIAL_STEP2_NODE_HPP
