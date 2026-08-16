#include "VehiclePresentationVerify.hpp"
#include "TutorialScenarioPresentation.hpp"
#include "platform/null/NullPlatformContext.hpp"

#include <cstdio>

int main()
{
  NullPlatformContext context;
  MyAppConfig production(&context);
  loka::scenario_tests::TutorialScenarioPresentation startupVehicle(&context, true);
  loka::scenario_tests::TutorialScenarioPresentation interactionVehicle(&context, false);
  loka::scenario_tests::VerifyVehiclePresentation(&context, production, startupVehicle, true);
  loka::scenario_tests::VerifyVehiclePresentation(&context, production, interactionVehicle, true);
  std::printf("testTutorialVehiclePresentationUsesExampleDeclaration passed\n");
  return 0;
}
