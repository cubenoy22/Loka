#ifndef DECLARA_CORE_PROFILER_HPP
#define DECLARA_CORE_PROFILER_HPP

#include <cassert>
#include <cstdio>

namespace declara
{
  namespace core
  {
    // Platform-specific backend interface
    struct ProfilerBackend
    {
      long (*getTicks)();
    };

    // Set by platform code at startup
    extern ProfilerBackend *gProfilerBackend;

    // Get current ticks (0 if no backend registered)
    inline long ProfileTicks()
    {
      return gProfilerBackend ? gProfilerBackend->getTicks() : 0;
    }

    // Profile entry storage
    struct ProfileEntry
    {
      const char *name;
      long ticks;
    };

    // Shared profile data (max 32 entries)
    extern ProfileEntry gProfileData[32];
    extern int gProfileCount;

    // Scoped profiler - records duration on destruction
    struct ScopedProfile
    {
      const char *name_;
      long start_;

      ScopedProfile(const char *name)
          : name_(name), start_(ProfileTicks()) {}

      ~ScopedProfile()
      {
        if (gProfileCount < 32)
        {
          gProfileData[gProfileCount].name = name_;
          gProfileData[gProfileCount].ticks = ProfileTicks() - start_;
          gProfileCount++;
        }
      }
    };

    // Manual checkpoint marker
    inline void ProfileCheckpoint(const char *name)
    {
      if (gProfileCount < 32)
      {
        gProfileData[gProfileCount].name = name;
        gProfileData[gProfileCount].ticks = ProfileTicks();
        gProfileCount++;
      }
    }

    // ============================================================
    // Function-level profiler (static slot registration)
    // ============================================================
#ifndef LOKA_PROFILE_FUNC_TICKS
#define LOKA_PROFILE_FUNC_TICKS 0
#endif

    struct FuncProfileSlot
    {
      const char *file;
      const char *func;
      int line;
      int callCount;
      long totalTicks;
    };

    enum { kMaxProfileSlots = 256 };
    extern FuncProfileSlot *gProfileSlots[kMaxProfileSlots];
    extern int gProfileSlotCount;

    // Register slot (called once per static slot)
    inline int RegisterProfileSlot(FuncProfileSlot *slot)
    {
      assert(gProfileSlotCount < kMaxProfileSlots && "Profile slot overflow");
      gProfileSlots[gProfileSlotCount] = slot;
      return gProfileSlotCount++;
    }

    struct FuncProfileScope
    {
      FuncProfileSlot *slot_;
      long start_;

      FuncProfileScope(FuncProfileSlot *slot)
          : slot_(slot), start_(ProfileTicks())
      {
        ++slot_->callCount;
      }

      ~FuncProfileScope()
      {
        slot_->totalTicks += ProfileTicks() - start_;
      }
    };

    inline void ResetFuncProfile()
    {
      for (int i = 0; i < gProfileSlotCount; ++i)
      {
        gProfileSlots[i]->callCount = 0;
        gProfileSlots[i]->totalTicks = 0;
      }
    }

    // Sort slots by totalTicks descending
    inline void SortProfileSlotsByTicks()
    {
      for (int i = 0; i < gProfileSlotCount - 1; ++i)
      {
        for (int j = i + 1; j < gProfileSlotCount; ++j)
        {
          if (gProfileSlots[j]->totalTicks > gProfileSlots[i]->totalTicks)
          {
            FuncProfileSlot *tmp = gProfileSlots[i];
            gProfileSlots[i] = gProfileSlots[j];
            gProfileSlots[j] = tmp;
          }
        }
      }
    }

    // Stream output interface
    struct ProfileOutputStream
    {
      virtual ~ProfileOutputStream() {}
      virtual void write(const char *data, int len) = 0;
    };

    // Dump to stream (no full memory allocation)
    // Uses \r for Classic Mac line endings
    inline void DumpFuncProfileToStream(ProfileOutputStream &out)
    {
      SortProfileSlotsByTicks();
      char buf[256];
      for (int i = 0; i < gProfileSlotCount; ++i)
      {
        FuncProfileSlot *s = gProfileSlots[i];
        if (s->callCount == 0) continue;
        int len = snprintf(buf, sizeof(buf), "%s:%d\t%s\t%d\t%ld\r",
                           s->file, s->line, s->func, s->callCount, s->totalTicks);
        out.write(buf, len);
      }
    }

#if LOKA_PROFILE_FUNC_TICKS
#define PROFILE_FUNC() \
  static ::declara::core::FuncProfileSlot _pslot_ = {__FILE__, __func__, __LINE__, 0, 0}; \
  static int _pslot_reg_ = ::declara::core::RegisterProfileSlot(&_pslot_); \
  ::declara::core::FuncProfileScope _pscope_(&_pslot_)
#define PROFILE_SECTION(name) \
  static ::declara::core::FuncProfileSlot _psec_##__LINE__ = {__FILE__, name, __LINE__, 0, 0}; \
  static int _psec_reg_##__LINE__ = ::declara::core::RegisterProfileSlot(&_psec_##__LINE__); \
  ::declara::core::FuncProfileScope _pscope_##__LINE__(&_psec_##__LINE__)
#else
#define PROFILE_FUNC()
#define PROFILE_SECTION(name)
#endif

    // ============================================================

    // Cumulative timing for compose phases
    extern long gComposeAttachTicks;
    extern long gComposeNodeTicks;
    extern long gComposeCreateTicks;

    // Cumulative timing for composeTree breakdown
    extern long gTreeVirtTicks;   // virtual calls (asBoundary, asComposable, asNestable)
    extern long gTreeCtxTicks;    // ComponentContext setup
    extern long gTreeCompTicks;   // compose() calls
    extern int gTreeNodeCount;    // number of nodes traversed

    // Detailed breakdown counters
    extern long gStateAllocTicks;  // useState allocation
    extern long gStateAddTicks;    // addState (duplicate check + register)
    extern long gNodeAllocTicks;   // node allocation in arena
    extern long gNodeCtorTicks;    // node constructor calls
    extern int gStateCount;        // number of states created
    extern int gNodeAllocCount;    // number of nodes allocated

    // DSL operation counters
    extern long gDefCloneTicks;    // definition clone (store)
    extern long gChildAddTicks;    // children add operations
    extern int gDefCloneCount;     // number of definitions cloned

    // Compose overhead counters
    extern long gClearChildTicks;  // clearChildren
    extern long gBeginCompTicks;   // beginComposition
    extern long gAddChildTicks;    // addChild after create
    extern long gCtxScopeTicks;    // ContextScope + attached setup
    extern long gCtxDtorTicks;     // ContextScope destructor
    extern int gComposeCallCount;  // number of compose() calls

    // Layout/Render counters
    extern long gLayoutTicks;      // LayoutNode total
    extern long gRenderTicks;      // RenderNode total

    // BMI debug counters
    extern long gBmiDeclTicks;     // declareStates
    extern long gBmiBindTicks;     // bind calls
    extern long gBmiUpdTicks;      // updateBmi
    extern long gBmiDslTicks;      // composeNode DSL

    // StateBatch breakdown counters
    extern long gBatchNewTicks;    // StateBatch ctor + .state() news
    extern long gBatchAppTicks;    // apply() loop in finalize
    extern long gBatchDelTicks;    // delete cleanup in finalize

    // Record cumulative compose timings to profile data
    inline void RecordComposeTicks()
    {
      if (gProfileCount < 24)
      {
        // Layout/Render first (most visible)
        gProfileData[gProfileCount].name = "Lyout";
        gProfileData[gProfileCount].ticks = gLayoutTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "Rendr";
        gProfileData[gProfileCount].ticks = gRenderTicks;
        gProfileCount++;
        // BMI breakdown
        gProfileData[gProfileCount].name = "bDecl";
        gProfileData[gProfileCount].ticks = gBmiDeclTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "bBind";
        gProfileData[gProfileCount].ticks = gBmiBindTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "bUpd";
        gProfileData[gProfileCount].ticks = gBmiUpdTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "bDsl";
        gProfileData[gProfileCount].ticks = gBmiDslTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "sbNew";
        gProfileData[gProfileCount].ticks = gBatchNewTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "sbApp";
        gProfileData[gProfileCount].ticks = gBatchAppTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "sbDel";
        gProfileData[gProfileCount].ticks = gBatchDelTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "cAtch";
        gProfileData[gProfileCount].ticks = gComposeAttachTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "cNode";
        gProfileData[gProfileCount].ticks = gComposeNodeTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "cCrea";
        gProfileData[gProfileCount].ticks = gComposeCreateTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "tVirt";
        gProfileData[gProfileCount].ticks = gTreeVirtTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "tCtx";
        gProfileData[gProfileCount].ticks = gTreeCtxTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "tComp";
        gProfileData[gProfileCount].ticks = gTreeCompTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "nodes";
        gProfileData[gProfileCount].ticks = gTreeNodeCount;
        gProfileCount++;
        // Detailed breakdown
        gProfileData[gProfileCount].name = "sAloc";
        gProfileData[gProfileCount].ticks = gStateAllocTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "sAdd";
        gProfileData[gProfileCount].ticks = gStateAddTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "nAloc";
        gProfileData[gProfileCount].ticks = gNodeAllocTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "nCtor";
        gProfileData[gProfileCount].ticks = gNodeCtorTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "sCnt";
        gProfileData[gProfileCount].ticks = gStateCount;
        gProfileCount++;
        gProfileData[gProfileCount].name = "nCnt";
        gProfileData[gProfileCount].ticks = gNodeAllocCount;
        gProfileCount++;
        // DSL operations
        gProfileData[gProfileCount].name = "dClon";
        gProfileData[gProfileCount].ticks = gDefCloneTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "dCnt";
        gProfileData[gProfileCount].ticks = gDefCloneCount;
        gProfileCount++;
        // Compose overhead
        gProfileData[gProfileCount].name = "cClr";
        gProfileData[gProfileCount].ticks = gClearChildTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "cBgn";
        gProfileData[gProfileCount].ticks = gBeginCompTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "cAdd";
        gProfileData[gProfileCount].ticks = gAddChildTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "cScop";
        gProfileData[gProfileCount].ticks = gCtxScopeTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "cDtor";
        gProfileData[gProfileCount].ticks = gCtxDtorTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "cCall";
        gProfileData[gProfileCount].ticks = gComposeCallCount;
        gProfileCount++;
      }
      gComposeAttachTicks = 0;
      gComposeNodeTicks = 0;
      gComposeCreateTicks = 0;
      gTreeVirtTicks = 0;
      gTreeCtxTicks = 0;
      gTreeCompTicks = 0;
      gTreeNodeCount = 0;
      gStateAllocTicks = 0;
      gStateAddTicks = 0;
      gNodeAllocTicks = 0;
      gNodeCtorTicks = 0;
      gStateCount = 0;
      gNodeAllocCount = 0;
      gDefCloneTicks = 0;
      gChildAddTicks = 0;
      gDefCloneCount = 0;
      gClearChildTicks = 0;
      gBeginCompTicks = 0;
      gAddChildTicks = 0;
      gCtxScopeTicks = 0;
      gCtxDtorTicks = 0;
      gComposeCallCount = 0;
      gLayoutTicks = 0;
      gRenderTicks = 0;
      gBmiDeclTicks = 0;
      gBmiBindTicks = 0;
      gBmiUpdTicks = 0;
      gBmiDslTicks = 0;
      gBatchNewTicks = 0;
      gBatchAppTicks = 0;
      gBatchDelTicks = 0;
    }

  } // namespace core
} // namespace declara

#endif // DECLARA_CORE_PROFILER_HPP
