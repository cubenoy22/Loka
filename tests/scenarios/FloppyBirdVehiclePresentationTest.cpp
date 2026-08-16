#include "VehiclePresentationVerify.hpp"
#include "FloppyBirdScenarioPresentation.hpp"
#include "platform/null/NullPlatformContext.hpp"

#include <cstdio>

int main()
{
  NullPlatformContext context;
  const unsigned long gameSeed = 0x27182818UL;
  MyAppConfig production(&context, gameSeed);
  loka::scenario_tests::FloppyBirdScenarioPresentation vehicle(&context, gameSeed);
  loka::scenario_tests::VerifyVehiclePresentation(&context, production, vehicle, true);
  std::printf("testFloppyBirdVehiclePresentationUsesExampleDeclaration passed\n");
  return 0;
}
