#ifndef LOKA_TUTORIAL_STEP3_NODE_HPP
#define LOKA_TUTORIAL_STEP3_NODE_HPP

#include "TutorialShared.hpp"
#include "app/Button.hpp"
#include "app/RowColumn.hpp"
#include "app/Show.hpp"
#include "app/Text.hpp"
#include "app/scene/BoundState.hpp"
#include "app/scene/node/StaticComposition.hpp"
#include "loka/core/State.hpp"
#include "loka/core/String.hpp"

namespace tutorial
{
  class Step3Node : public loka::app::scene::BoundaryNodeFor<Step3Node>
  {
  public:
    typedef loka::app::scene::BoundaryPropsFor<Step3Node> PropsType;

    Step3Node(const PropsType &p)
        : loka::app::scene::BoundaryNodeFor<Step3Node>(p),
          showDetails_(),
          toggleDetailsEvent_(),
          initialized_(false),
          details_(loka::app::Text("Conditional content is visible."))
    {
    }

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      if (this->initialized_)
      {
        return;
      }
      c.declareStates()
          .state(this->showDetails_, false);
      this->bindForUi(this->toggleDetailsEvent_, this, &Step3Node::toggleDetails);
      this->initialized_ = true;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      c.declare(VStack()
                << TutorialTitle("Step 3")
                << loka::app::Button("Toggle details", &this->toggleDetailsEvent_)
                << (Show(*this->showDetails_.state()) << this->details_)
                << TutorialHint("Show(condition) keeps conditional UI readable in the DSL."));
    }

  private:
    void toggleDetails()
    {
      if (!this->showDetails_.isValid())
      {
        return;
      }
      this->showDetails_.set(!this->showDetails_.get(), true);
    }

    loka::app::scene::BoundState<bool> showDetails_;
    loka::core::EmitterState toggleDetailsEvent_;
    bool initialized_;
    loka::app::TextDefinition details_;
  };
} // namespace tutorial

#endif // LOKA_TUTORIAL_STEP3_NODE_HPP
