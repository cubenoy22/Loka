#ifndef LOKA_DSL_COMPOSITION_DIFF_HPP
#define LOKA_DSL_COMPOSITION_DIFF_HPP

namespace loka
{
  namespace dsl
  {
    struct CompositionDiff
    {
      CompositionDiff() : valid(false), fullRebuild(true) {}
      void clear()
      {
        valid = false;
        fullRebuild = true;
      }

      bool valid;
      bool fullRebuild;
    };
  } // namespace dsl
} // namespace loka

#endif // LOKA_DSL_COMPOSITION_DIFF_HPP
