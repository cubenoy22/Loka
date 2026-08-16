#include "ClassicVehiclePresentationVerify.hpp"
#include "FloppyBirdClassicScenarioPresentation.hpp"
#include "platform/null/NullPlatformContext.hpp"

#include <cstdio>

int main()
{
  NullPlatformContext context;
  const unsigned long gameSeed = 0x27182818UL;
  MyAppConfig production(&context, gameSeed);
  loka::scenario_tests::FloppyBirdClassicScenarioPresentation vehicle(&context, gameSeed);
  loka::scenario_tests::VerifyClassicVehiclePresentation(&context, production, vehicle, true);
  std::printf("testFloppyBirdClassicVehiclePresentationUsesExampleDeclaration passed\n");
  return 0;
}
