# Common translation units for every Loka unit-test executable. This file is
# included by both the repository root and standalone example projects.
set(_LOKA_TEST_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
set(LOKA_SHARED_TEST_SOURCES
  ${_LOKA_TEST_SOURCE_ROOT}/tests/TestingHooks.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/platform/null/NullPlatformContext.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/platform/null/NullScenePlatformController.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/platform/null/context/NullButtonContext.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/platform/null/context/NullEditTextContext.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/NullPlatformContractTests.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/LifecycleFactTests.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/StateNotifyTests.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/StateTrackerCommitTests.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/DerivedStateTests.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/DefinitionCloneTests.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/SceneOwnershipTests.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/PhaseGuardTests.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/LifecycleDetachTests.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/NativeLifetimeTests.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/ValueTests.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/LokaAllocTests.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/SnapFormatTests.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/FlowDslTests.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/AttrDslTests.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/BoundaryArenaTests.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/StartupRedrawTests.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/tests/FlowDslTestMain.cpp
  ${_LOKA_TEST_SOURCE_ROOT}/example/HelloWorld/src/MainNode.cpp
)
unset(_LOKA_TEST_SOURCE_ROOT)
