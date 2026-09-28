#ifndef TOOLBOX_PROFILER_HPP
#define TOOLBOX_PROFILER_HPP

/** Low-word microseconds for cumulative diagnostic pass timing (<71 minutes). */
#if LOKA_PROFILE_FUNC_TICKS
unsigned long ToolboxProfileMicroseconds();
#else
inline unsigned long ToolboxProfileMicroseconds() { return 0; }
#endif

// Initialize the Toolbox profiler backend (call at startup).
void InitToolboxProfiler();

#endif // TOOLBOX_PROFILER_HPP
