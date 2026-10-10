#ifndef LOKA_TOOLBOX_RESERVE_GROW_ZONE_HPP
#define LOKA_TOOLBOX_RESERVE_GROW_ZONE_HPP

#include <MacMemory.h>

namespace loka { namespace toolbox {
/** Register the process-lived reserve procedure once after successful arming. */
void InstallReserveGrowZone(GrowZoneUPP procedure);
/** An allocation inside refuses with null instead of spending the OOM reserve.
    Used only around a synchronous Memory Manager request; scopes do not nest. */
class RefusingAllocationScope
{
public:
  RefusingAllocationScope();
  ~RefusingAllocationScope();
private:
  RefusingAllocationScope(const RefusingAllocationScope &);
  RefusingAllocationScope &operator=(const RefusingAllocationScope &);
};
} }
#endif
