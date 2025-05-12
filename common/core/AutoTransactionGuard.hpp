#pragma once
#include "StateTracker.hpp"

// StateTracker用RAIIトランザクションガード
struct AutoTransactionGuard
{
  PushStateTracker *tracker;
  AutoTransactionGuard(StateTracker *t)
      : tracker(dynamic_cast<PushStateTracker *>(t))
  {
    if (tracker)
      tracker->begin();
  }
  ~AutoTransactionGuard()
  {
    if (tracker)
      tracker->end();
  }
};
