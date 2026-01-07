#ifndef LOKA_DSL_REFRESH_LOOP_HPP
#define LOKA_DSL_REFRESH_LOOP_HPP

namespace loka
{
  namespace dsl
  {
    class RefreshLoop
    {
    public:
      typedef bool (*RefreshFn)(void *);
      typedef void (*ApplyFn)(void *);

      RefreshLoop() : requested_(false), inProgress_(false), maxIterations_(100) {}

      void request() { requested_ = true; }
      bool inProgress() const { return inProgress_; }

      void setMaxIterations(int maxIterations)
      {
        maxIterations_ = maxIterations;
      }

      bool run(RefreshFn refresh, ApplyFn apply, void *userData)
      {
        if (inProgress_)
        {
          return false;
        }
        inProgress_ = true;
        bool changed = false;
        int iterations = 0;
        while (requested_ && iterations < maxIterations_)
        {
          requested_ = false;
          if (refresh(userData))
          {
            changed = true;
          }
          ++iterations;
        }
        inProgress_ = false;
        if (changed)
        {
          apply(userData);
        }
        return changed;
      }

      template <class RefreshFn, class ApplyFn>
      bool run(RefreshFn refresh, ApplyFn apply)
      {
        if (inProgress_)
        {
          return false;
        }
        inProgress_ = true;
        bool changed = false;
        int iterations = 0;
        while (requested_ && iterations < maxIterations_)
        {
          requested_ = false;
          if (refresh())
          {
            changed = true;
          }
          ++iterations;
        }
        inProgress_ = false;
        if (changed)
        {
          apply();
        }
        return changed;
      }

    private:
      bool requested_;
      bool inProgress_;
      int maxIterations_;
    };
  } // namespace dsl
} // namespace loka

#endif // LOKA_DSL_REFRESH_LOOP_HPP
