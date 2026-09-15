#ifndef LOKA_MAC_RETIRABLE_CONTEXT_HPP
#define LOKA_MAC_RETIRABLE_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"

class MacScenePlatformController;

/** Transfers retained Objective-C projection objects to the controller's
    safe-point queue before the synchronous C++ context reclaim. */
class MacRetirableContext : public loka::app::scene::NativeNodeContext,
                            public loka::app::scene::IBoundaryTaggedContext
{
public:
  explicit MacRetirableContext(MacScenePlatformController *controller);
  virtual ~MacRetirableContext();
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

protected:
  void retireNativeObjects(void *&primary, void *&auxiliary);
  void retireNativeObject(void *&primary);

private:
  loka::app::scene::BoundaryNode *boundary_;
  MacScenePlatformController *controller_;
};

#endif // LOKA_MAC_RETIRABLE_CONTEXT_HPP
