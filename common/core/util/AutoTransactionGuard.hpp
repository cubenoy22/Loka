#ifndef DECLARA_UTIL_AUTOTRANSACTIONGUARD_HPP
#define DECLARA_UTIL_AUTOTRANSACTIONGUARD_HPP
#include "core/StateTracker.hpp"

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

#endif // DECLARA_UTIL_AUTOTRANSACTIONGUARD_HPP
  