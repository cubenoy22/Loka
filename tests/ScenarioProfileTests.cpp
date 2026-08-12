#include "ScenarioProfileTests.hpp"

#include <cassert>
#include <string>

#include "scenarios/ScenarioProfile.hpp"

void testScenarioProfileV2CarriesAvailabilityWithoutInventingValues()
{
  typedef loka::scenario_tests::ProfileFact<int> IntFact;
  typedef loka::scenario_tests::ProfileFact<std::string> StringFact;

  std::string untouched = "sentinel";
  assert(!StringFact::unavailable().query(untouched));
  assert(untouched == "sentinel");
  assert(StringFact::available("dark").query(untouched));
  assert(untouched == "dark");

  const loka::scenario_tests::ScenarioProfile profile("25G72",
                                                      "x86_64",
                                                      IntFact::available(200),
                                                      IntFact::available(32),
                                                      StringFact::unavailable(),
                                                      "NSView.cacheDisplayInRect.v1",
                                                      680,
                                                      500);
  const std::string expected = "profile_version=2\n"
                               "os_build=25G72\n"
                               "arch=x86_64\n"
                               "scale_percent_available=1\n"
                               "scale_percent=200\n"
                               "depth_available=1\n"
                               "depth=32\n"
                               "appearance_available=0\n"
                               "capture_api=NSView.cacheDisplayInRect.v1\n"
                               "pixel_width=680\n"
                               "pixel_height=500\n";
  assert(profile.render() == expected);
  assert(profile.render().find("appearance=") == std::string::npos);

  const loka::scenario_tests::ScenarioProfile unavailableProfile("10K549",
                                                                 "i386",
                                                                 IntFact::unavailable(),
                                                                 IntFact::unavailable(),
                                                                 StringFact::unavailable(),
                                                                 "NSView.cacheDisplayInRect.v1",
                                                                 340,
                                                                 250);
  const std::string unavailableExpected = "profile_version=2\n"
                                          "os_build=10K549\n"
                                          "arch=i386\n"
                                          "scale_percent_available=0\n"
                                          "depth_available=0\n"
                                          "appearance_available=0\n"
                                          "capture_api=NSView.cacheDisplayInRect.v1\n"
                                          "pixel_width=340\n"
                                          "pixel_height=250\n";
  assert(unavailableProfile.render() == unavailableExpected);
  assert(unavailableProfile.render().find("scale_percent=") == std::string::npos);
  assert(unavailableProfile.render().find("depth=") == std::string::npos);
  assert(unavailableProfile.render().find("appearance=") == std::string::npos);

  const loka::scenario_tests::ScenarioProfile modernProfile("25G72",
                                                            "x86_64",
                                                            IntFact::available(200),
                                                            IntFact::available(32),
                                                            StringFact::available("dark"),
                                                            "NSView.cacheDisplayInRect.v1",
                                                            680,
                                                            500);
  assert(modernProfile.render().find("appearance_available=1\nappearance=dark\n") != std::string::npos);
}
