#include "VehiclePresentationVerify.hpp"
#include "MineSweeperScenarioPresentation.hpp"
#include "platform/null/NullPlatformContext.hpp"

#include <cstdio>

int main()
{
  NullPlatformContext context;
  const minesweeper::MainProps mainProps(0x31415926UL);
  MyAppConfig production(&context, mainProps);
  loka::scenario_tests::MineSweeperScenarioPresentation vehicle(&context, mainProps);
  loka::scenario_tests::VerifyVehiclePresentation(&context, production, vehicle, false);
  std::printf("testMineSweeperVehiclePresentationUsesExampleDeclaration passed\n");
  return 0;
}
