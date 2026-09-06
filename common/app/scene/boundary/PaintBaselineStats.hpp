#ifndef LOKA_SCENE_PAINT_BASELINE_STATS_HPP
#define LOKA_SCENE_PAINT_BASELINE_STATS_HPP

#ifdef TEST_BUILD
namespace loka
{
  namespace app
  {
    namespace scene
    {
      namespace testing
      {
        /** Test-only cumulative call counts. Subtract snapshots around one synchronous
            Scene cycle; no reset can erase another observer's measurement. Counts are
            process-wide, so the measured interval must drive only the intended Scene. */
        struct PaintBaselineStats
        {
          PaintBaselineStats()
              : boundaryUpdateVisits(0),
                dirtySourceDeclarations(0),
                boundaryApplyCallbacks(0)
          {
          }
          unsigned long boundaryUpdateVisits;
          unsigned long dirtySourceDeclarations;
          unsigned long boundaryApplyCallbacks;
        };

        inline PaintBaselineStats &paintBaselineStats()
        {
          static PaintBaselineStats stats;
          return stats;
        }
      } // namespace testing
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
#endif
