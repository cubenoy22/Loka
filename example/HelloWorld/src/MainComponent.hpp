#ifndef LOKA_HELLOWORLD_MAIN_COMPONENT_HPP
#define LOKA_HELLOWORLD_MAIN_COMPONENT_HPP

#include "MainNode.hpp"
#include "MainLeftPanel.hpp"
#include "MainRightPanel.hpp"
#include "app/RowColumn.hpp"
#include "core2/scene/Component.hpp"

namespace helloworld
{
  inline void MainNode::composeNode(declara::core::scene::NodeComposition &c)
  {
    using namespace declara::app;
    MainLeftPanelComponent leftPanel(this);
    MainRightPanelComponent rightPanel(this);
    c.declare(
        HStack()
        << declara::core::scene::LightComponent(leftPanel)
        << declara::core::scene::LightComponent(rightPanel));
  }
} // namespace helloworld

#endif // LOKA_HELLOWORLD_MAIN_COMPONENT_HPP
