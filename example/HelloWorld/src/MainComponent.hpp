#ifndef LOKA_HELLOWORLD_MAIN_COMPONENT_HPP
#define LOKA_HELLOWORLD_MAIN_COMPONENT_HPP

#include "MainNode.hpp"
#include "MainLeftPanel.hpp"
#include "MainRightPanel.hpp"
#include "app/RowColumn.hpp"

namespace helloworld
{
  inline void MainNode::composeNode(declara::core::scene::NodeComposition &c)
  {
    using namespace declara::app;
    c.declare(
        HStack()
        << declara::core::scene::NodeDefinition<MainLeftPanelProps, MainLeftPanelNode>(MainLeftPanelProps(this))
        << declara::core::scene::NodeDefinition<MainRightPanelProps, MainRightPanelNode>(MainRightPanelProps(this)));
  }
} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_COMPONENT_HPP
