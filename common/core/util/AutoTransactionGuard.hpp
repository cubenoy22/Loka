#ifndef DECLARA_UTIL_AUTOTRANSACTIONGUARD_HPP
#define DECLARA_UTIL_AUTOTRANSACTIONGUARD_HPP
#include "core/StateTracker.hpp"

// StateTracker用RAIIトランザクションガード
struct AutoTransactionGuard
{
  declara::core::PushStateTracker *tracker;
  AutoTransactionGuard(declara::core::StateTracker *t)
      : tracker(dynamic_cast<declara::core::PushStateTracker *>(t))
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
  
