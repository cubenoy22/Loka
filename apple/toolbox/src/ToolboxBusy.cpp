#include "ToolboxBusy.hpp"
#include <cassert>

namespace
{
  ToolboxBusyOwner *gRegisteredBusyOwner = 0;
}

ToolboxBusyOwnerRegistration::ToolboxBusyOwnerRegistration(ToolboxBusyOwner &owner)
{
  assert(!gRegisteredBusyOwner && "one busy owner per process");
  gRegisteredBusyOwner = &owner;
}

ToolboxBusyOwnerRegistration::~ToolboxBusyOwnerRegistration()
{
  gRegisteredBusyOwner = 0;
}

ToolboxBusyOwner *RegisteredToolboxBusyOwner()
{
  return gRegisteredBusyOwner;
}
