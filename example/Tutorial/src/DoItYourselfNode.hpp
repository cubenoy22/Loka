#ifndef LOKA_TUTORIAL_DO_IT_YOURSELF_NODE_HPP
#define LOKA_TUTORIAL_DO_IT_YOURSELF_NODE_HPP

#include "TutorialShared.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/boundary/StdComposition.hpp"

namespace tutorial
{
  class DoItYourselfNode : public loka::app::scene::BoundaryNodeFor<DoItYourselfNode>
  {
  public:
    typedef loka::app::scene::BoundaryPropsFor<DoItYourselfNode> PropsType;

    DoItYourselfNode(const PropsType &p)
        : loka::app::scene::BoundaryNodeFor<DoItYourselfNode>(p)
    {
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      using namespace loka::app;
      c.declare(                                                             //
          VStack()                                                           //
          << TutorialTitle("Loka Tutorial")                                  //
          << Text("Edit DoItYourselfNode.hpp and build the scene yourself.") //
          << TutorialHint("Then switch TutorialNode in MyAppConfig.hpp to Step1Node ... Step4Node for answers.")
          << Text("Suggested order:")           //
          << Text("1. Show Hello, Loka")        //
          << Text("2. Add an Increment button") //
          << Text("3. Add Show(condition) details")
          << Text("4. Add a revealed item list and update derived summary text"));
    }
  };
} // namespace tutorial

#endif // LOKA_TUTORIAL_DO_IT_YOURSELF_NODE_HPP
