#include "context/ToolboxProjectedNodeContext.hpp"

#include "ToolboxScenePlatformController.hpp"

void ToolboxProjectedNodeContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                                                loka::app::scene::NodeLifecycleFact next)
{
  (void)previous;
  if (next != loka::app::scene::NODE_FACT_RETIRED || !this->controller_)
  {
    return;
  }
  this->retireNativeProjection();
  this->controller_->retireNodeContext(this, this->lifetimeHint());
  this->controller_ = 0;
  this->boundary_ = 0;
}
