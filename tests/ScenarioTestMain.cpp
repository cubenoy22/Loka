#include "FlowDslTests.hpp"
#include "OwnershipDumpTests.hpp"
#include "HelloWorldScenarioTests.hpp"
#if defined(LOKA_BUILD_SMIRKYCARD)
#include "SmirkyCardScenarioTests.hpp"
#endif
#include "TutorialScenarioTests.hpp"
#include "MineSweeperScenarioTests.hpp"
#include "FloppyBirdScenarioTests.hpp"
#include "StartupScenarioTests.hpp"
#include "ScenarioReelTests.hpp"
#include "StandalonePerformanceTests.hpp"

#define LOKA_TEST_RUNNER_SCENARIO
#define LOKA_TEST_RUNNER_FINAL_CHECKPOINT "ScenarioTestMain final"
#include "TestRunnerMain.inc"
