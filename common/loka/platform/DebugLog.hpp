#ifndef LOKA_PLATFORM_DEBUG_LOG_HPP
#define LOKA_PLATFORM_DEBUG_LOG_HPP

namespace loka
{
  namespace platform
  {
    void DebugLogRecomposeTracked(void *boundary, void *scene);
    void DebugLogRecomposeQueued(void *scene);
  } // namespace platform
} // namespace loka

#endif // LOKA_PLATFORM_DEBUG_LOG_HPP
