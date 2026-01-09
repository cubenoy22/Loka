#import <Foundation/Foundation.h>
#include "loka/platform/DebugLog.hpp"

namespace loka
{
  namespace platform
  {
    void DebugLogRecomposeTracked(void *boundary, void *scene)
    {
      NSLog(@"[recompose] update tracked, queue scene refresh (boundary=%p scene=%p)", boundary, scene);
    }

    void DebugLogRecomposeQueued(void *scene)
    {
      NSLog(@"[recompose] queued update task (scene=%p)", scene);
    }
  } // namespace platform
} // namespace loka
