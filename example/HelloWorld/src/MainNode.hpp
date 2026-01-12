#ifndef LOKA_HELLOWORLD_MAIN_NODE_HPP
#define LOKA_HELLOWORLD_MAIN_NODE_HPP

#include "core2/scene/node/StaticComposition.hpp"
#include "MainLeftPanel.hpp"
#include "MainRightPanel.hpp"

class Window;

namespace helloworld
{
  class MainNode;
  typedef declara::core::scene::StaticCompositionPropsFor<MainNode> MainProps;

  class MainNode : public declara::core::scene::StaticCompositionNodeFor<MainNode>
  {
  public:
    MainNode(const MainProps &p)
        : declara::core::scene::StaticCompositionNodeFor<MainNode>(MainProps(p)),
          left_(this),
          right_(this)
    {
    }

    template <class NodeT>
    void bindForUiComponent(declara::core::EmitterState &emitter, NodeT *node, void (NodeT::*method)())
    {
      this->bindForUi(emitter, node, method);
    }

    ::Window *windowOrNull() const
    {
      const AttachedContext *ctx = this->attachedContext();
      return ctx ? ctx->window() : 0;
    }

    virtual void composeNode(declara::core::scene::NodeComposition &c);

  private:
    MainLeftPanelComponent left_;
    MainRightPanelComponent right_;
  };

  inline void MainNode::composeNode(declara::core::scene::NodeComposition &c)
  {
    using namespace declara::app;
    c.declare(
        HStack()
        << declara::core::scene::LC(left_)
        << declara::core::scene::LC(right_));
  }

} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_NODE_HPP
