#include "VehiclePresentationVerify.hpp"
#include "ScrapbookScenarioPresentation.hpp"
#include "platform/null/NullPlatformContext.hpp"

#include <cstdio>

int main()
{
  NullPlatformContext context;
  ScrapbookAppConfig production(&context);
  loka::scenario_tests::ScrapbookScenarioPresentation vehicle(&context);
  loka::scenario_tests::VerifyVehiclePresentation(&context, production, vehicle, true);
  std::printf("testScrapbookVehiclePresentationUsesExampleDeclaration passed\n");
  return 0;
}
