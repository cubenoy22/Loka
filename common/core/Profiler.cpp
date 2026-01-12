#include "Profiler.hpp"

namespace declara
{
  namespace core
  {
    ProfilerBackend *gProfilerBackend = 0;
    ProfileEntry gProfileData[32];
    int gProfileCount = 0;

    // Function-level profiler storage
    FuncProfileSlot *gProfileSlots[kMaxProfileSlots];
    int gProfileSlotCount = 0;

    // Cumulative timing for compose phases
    long gComposeAttachTicks = 0;
    long gComposeNodeTicks = 0;
    long gComposeCreateTicks = 0;

    // Cumulative timing for composeTree breakdown
    long gTreeVirtTicks = 0;
    long gTreeCtxTicks = 0;
    long gTreeCompTicks = 0;
    int gTreeNodeCount = 0;

    // Detailed breakdown counters
    long gStateAllocTicks = 0;
    long gStateAddTicks = 0;
    long gNodeAllocTicks = 0;
    long gNodeCtorTicks = 0;
    int gStateCount = 0;
    int gNodeAllocCount = 0;

    // DSL operation counters
    long gDefCloneTicks = 0;
    long gChildAddTicks = 0;
    int gDefCloneCount = 0;

    // Compose overhead counters
    long gClearChildTicks = 0;
    long gBeginCompTicks = 0;
    long gAddChildTicks = 0;
    long gCtxScopeTicks = 0;
    long gCtxDtorTicks = 0;
    int gComposeCallCount = 0;

    // Layout/Render counters
    long gLayoutTicks = 0;
    long gRenderTicks = 0;

    // BMI debug counters
    long gBmiDeclTicks = 0;
    long gBmiBindTicks = 0;
    long gBmiUpdTicks = 0;
    long gBmiDslTicks = 0;

    // StateBatch breakdown counters
    long gBatchNewTicks = 0;
    long gBatchAppTicks = 0;
    long gBatchDelTicks = 0;
  } // namespace core
} // namespace declara
