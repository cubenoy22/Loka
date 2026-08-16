#include "VehiclePresentationVerify.hpp"
#include "TutorialScenarioPresentation.hpp"
#include "platform/null/NullPlatformContext.hpp"

#include <cstdio>

int main()
{
  NullPlatformContext context;
  MyAppConfig production(&context);
  loka::scenario_tests::TutorialScenarioPresentation vehicle(&context, true);
  loka::scenario_tests::VerifyVehiclePresentation(&context, production, vehicle, true);
  std::printf("testTutorialVehiclePresentationUsesExampleDeclaration passed\n");
  return 0;
}
