#include "ClassicVehiclePresentationVerify.hpp"
#include "MineSweeperClassicScenarioPresentation.hpp"
#include "platform/null/NullPlatformContext.hpp"

#include <cstdio>

int main()
{
  NullPlatformContext context;
  const minesweeper::MainProps mainProps(0x31415926UL);
  MyAppConfig production(&context, mainProps);
  loka::scenario_tests::MineSweeperClassicScenarioPresentation vehicle(&context, mainProps);
  loka::scenario_tests::VerifyClassicVehiclePresentation(&context, production, vehicle, false);
  std::printf("testMineSweeperClassicVehiclePresentationUsesExampleDeclaration passed\n");
  return 0;
}
