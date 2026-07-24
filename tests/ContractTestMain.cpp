#include "StateNotifyTests.hpp"
#include "StateTrackerCommitTests.hpp"
#ifdef _WIN32
#include "Win32ThreadModalScopeTests.hpp"
#endif
#include "DerivedStateTests.hpp"
#include "DefinitionCloneTests.hpp"
#include "AttrDslTests.hpp"
#include "SnapFormatTests.hpp"
#include "SceneTests.hpp"
#include "StartupRedrawTests.hpp"
#include "BoundaryArenaTests.hpp"
#include "ValueTests.hpp"
#include "SceneOwnershipTests.hpp"
#include "PhaseGuardTests.hpp"
#include "LifecycleDetachTests.hpp"
#include "NativeLifetimeTests.hpp"
#include "NullPlatformContractTests.hpp"
#include "LifecycleFactTests.hpp"
#include "LokaAllocTests.hpp"
#include "core/diag/LifecycleAudit.hpp"

#include "ContractTestSupport.inc"

#define LOKA_TEST_RUNNER_CONTRACT
#define LOKA_TEST_RUNNER_FINAL_CHECKPOINT "ContractTestMain final"
#include "TestRunnerMain.inc"
