#ifndef LOKA_TOOLBOX_PROJECTED_NODE_CONTEXT_HPP
#define LOKA_TOOLBOX_PROJECTED_NODE_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"

class ToolboxScenePlatformController;

/** Base for projected Toolbox contexts. boundary_ is the non-owning
    "which window/root am I in" tag used when recording hits and text runs
    into the controller ledgers. The wall
    (ToolboxScenePlatformController::prepareProjectedLayout) re-applies it on
    every projection sweep, so cells never touch it. */
class ToolboxProjectedNodeContext : public loka::app::scene::NativeNodeContext,
                                    public loka::app::scene::IBoundaryTaggedContext
{
public:
  explicit ToolboxProjectedNodeContext(ToolboxScenePlatformController *controller)
      : loka::app::scene::NativeNodeContext(),
        boundary_(0),
        controller_(controller)
  {
  }

  void readLifecycleFactOnAttach()
  {
    // No per-context native presentation on Toolbox; the ledgers repaint from
    // projection sweeps — the method exists so the shared ensure ritual stays
    // uniform.
  }

  virtual loka::app::scene::IBoundaryTaggedContext *asBoundaryTagged()
  {
    return this;
  }

  virtual void setBoundary(loka::app::scene::BoundaryNode *boundary)
  {
    this->boundary_ = boundary;
  }

  virtual loka::app::scene::BoundaryNode *boundary() const
  {
    return this->boundary_;
  }

  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous, loka::app::scene::NodeLifecycleFact next);

protected:
  virtual void retireNativeProjection() {}
  ToolboxScenePlatformController *controller() const
  {
    return this->controller_;
  }

  loka::app::scene::BoundaryNode *boundary_;

private:
  ToolboxScenePlatformController *controller_;
};

#endif // LOKA_TOOLBOX_PROJECTED_NODE_CONTEXT_HPP
