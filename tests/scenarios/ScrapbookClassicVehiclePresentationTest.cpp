#include "ClassicVehiclePresentationVerify.hpp"
#include "ScrapbookClassicScenarioPresentation.hpp"
#include "platform/null/NullPlatformContext.hpp"

#include <cstdio>

int main()
{
  NullPlatformContext context;
  ScrapbookAppConfig production(&context);
  loka::scenario_tests::ScrapbookClassicScenarioPresentation vehicle(&context);
  loka::scenario_tests::VerifyClassicVehiclePresentation(&context, production, vehicle, true);
  std::printf("testScrapbookClassicVehiclePresentationUsesExampleDeclaration passed\n");
  return 0;
}
