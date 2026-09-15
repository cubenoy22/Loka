#ifndef LOKA_MAC_RETIRABLE_CONTEXT_HPP
#define LOKA_MAC_RETIRABLE_CONTEXT_HPP

#include "app/scene/projection/NativeNodeContext.hpp"

class MacScenePlatformController;

/** Transfers retained Objective-C projection objects to the controller's
    safe-point queue before the synchronous C++ context reclaim. */
class MacRetirableContext : public loka::app::scene::NativeNodeContext
{
public:
  explicit MacRetirableContext(MacScenePlatformController *controller);
  virtual ~MacRetirableContext();

protected:
  void retireNativeObjects(void *&primary, void *&auxiliary);
  void retireNativeObject(void *&primary);

private:
  MacScenePlatformController *controller_;
};

#endif // LOKA_MAC_RETIRABLE_CONTEXT_HPP
