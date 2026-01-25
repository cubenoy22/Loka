#ifndef LOKA_HELLOWORLD_MAIN_NODE_HPP
#define LOKA_HELLOWORLD_MAIN_NODE_HPP

#include "core2/scene/node/StaticComposition.hpp"
#include "MainLeftPanel.hpp"
#include "MainRightPanel.hpp"
#include "app/ZStack.hpp"
#include "app/Text.hpp"

class Window;

namespace helloworld
{
  class MainNode;
  typedef loka::core::scene::StaticCompositionPropsFor<MainNode> MainProps;

  class MainNode : public loka::core::scene::StaticCompositionNodeFor<MainNode>
  {
  public:
    MainNode(const MainProps &p)
        : loka::core::scene::StaticCompositionNodeFor<MainNode>(MainProps(p)),
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

    virtual void composeNode(loka::core::scene::NodeComposition &c);

  private:
    MainLeftPanelComponent left_;
    MainRightPanelComponent right_;
  };

  inline void MainNode::composeNode(loka::core::scene::NodeComposition &c)
  {
    using namespace loka::app;
    ZStack &root = c.declare(ZStack());
    loka::core::scene::NodeComposition::ParentScope scope(c, root);
    c.declare(HStack()
              << loka::core::scene::LC(left_)
              << loka::core::scene::LC(right_));
    c.declare(Text("*"));
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_NODE_HPP
