#include "ClassicVehiclePresentationVerify.hpp"
#include "TutorialClassicScenarioPresentation.hpp"
#include "platform/null/NullPlatformContext.hpp"

#include <cstdio>

int main()
{
  NullPlatformContext context;
  MyAppConfig production(&context);
  loka::scenario_tests::TutorialClassicScenarioPresentation vehicle(&context, true);
  loka::scenario_tests::VerifyClassicVehiclePresentation(&context, production, vehicle, true);
  std::printf("testTutorialClassicVehiclePresentationUsesExampleDeclaration passed\n");
  return 0;
}
