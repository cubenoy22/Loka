#ifndef LOKA_TUTORIAL_STEP1_NODE_HPP
#define LOKA_TUTORIAL_STEP1_NODE_HPP

#include "TutorialShared.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/Text.hpp"
#include "app/scene/nodes/boundary/StdComposition.hpp"

namespace tutorial {
  class Step1Node : public loka::app::scene::BoundaryNodeFor<Step1Node> {
  public:
    typedef loka::app::scene::BoundaryPropsFor<Step1Node> PropsType;

    Step1Node(const PropsType &p) : loka::app::scene::BoundaryNodeFor<Step1Node>(p) {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c) {
      using namespace loka::app;
      c.declare(                     //
          VStack()                   //
          << TutorialTitle("Step 1") //
          << Text("Hello, Loka")     //
          << TutorialHint("Start with a single Text node."));
    }
  };
} // namespace tutorial

#endif // LOKA_TUTORIAL_STEP1_NODE_HPP
