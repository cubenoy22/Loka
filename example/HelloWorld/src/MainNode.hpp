#ifndef LOKA_HELLOWORLD_MAIN_NODE_HPP
#define LOKA_HELLOWORLD_MAIN_NODE_HPP

#include "app/scene/Component.hpp"
#include "app/scene/node/StaticComposition.hpp"
#include "MainLeftPanel.hpp"
#include "MainRightPanel.hpp"
#include "app/ZStack.hpp"
#include "app/Text.hpp"

class Window;

namespace helloworld
{
  class MainNode;
  typedef loka::app::scene::StaticCompositionPropsFor<MainNode> MainProps;

  class MainNode : public loka::app::scene::StaticCompositionNodeFor<MainNode>
  {
  public:
    MainNode(const MainProps &p)
        : loka::app::scene::StaticCompositionNodeFor<MainNode>(MainProps(p)),
          left_(this),
          right_(this)
    {
    }

    template <class NodeT>
    void bindForUiComponent(loka::core::EmitterState &emitter, NodeT *node, void (NodeT::*method)())
    {
      this->bindForUi(emitter, node, method);
    }

    ::Window *windowOrNull() const
    {
      const AttachedContext *ctx = this->attachedContext();
      return ctx ? ctx->window() : 0;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c);

  private:
    MainLeftPanelComponent left_;
    MainRightPanelComponent right_;
  };

  inline void MainNode::composeNode(loka::app::scene::NodeComposition &c)
  {
    using namespace loka::app;
    ZStack &root = c.declare(ZStack().TEST_ID("HelloWorld.Root"));
    loka::app::scene::NodeComposition::ParentScope scope(c, root);
    c.declare(HStack().TEST_ID("HelloWorld.MainPanels")
              << loka::app::scene::LightComponent(left_)
              << loka::app::scene::LightComponent(right_));
    c.declare(Text("*").TEST_ID("HelloWorld.Decoration"));
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_NODE_HPP
