#include "ToolboxReserveGrowZone.hpp"

namespace {
// System 7's Process Manager keeps its wrapper in Zone::gzProc. Only the
// application procedure passed to SetGrowZone can be registered again safely.
GrowZoneUPP gReserveGrowZone;
}

namespace loka { namespace toolbox {
void InstallReserveGrowZone(GrowZoneUPP procedure)
{
  gReserveGrowZone = procedure;
  SetGrowZone(procedure);
}

RefusingAllocationScope::RefusingAllocationScope()
{
  SetGrowZone(0);
}

RefusingAllocationScope::~RefusingAllocationScope()
{
  SetGrowZone(gReserveGrowZone);
}
} }
