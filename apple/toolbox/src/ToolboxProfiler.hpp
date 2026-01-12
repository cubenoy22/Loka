#ifndef TOOLBOX_PROFILER_HPP
#define TOOLBOX_PROFILER_HPP

#include "core/Profiler.hpp"
#include <string>

// Re-export common profiler types for convenience
using declara::core::ProfileEntry;
using declara::core::ScopedProfile;
using declara::core::RecordComposeTicks;
using declara::core::gProfileData;
using declara::core::gProfileCount;
using declara::core::gLayoutTicks;
using declara::core::gRenderTicks;

// Toolbox-specific: result string for display after BuildProfileResultString
extern std::string gProfileResultString;

// Initialize the Toolbox profiler backend (call at startup)
void InitToolboxProfiler();

// Build result string from profile data and reset count
void BuildProfileResultString();

// Dump function-level profiler data to a file (call with filename like "profile.txt")
// Returns true if file was written successfully
bool DumpFuncProfileToFile(const char *filename);

#endif // TOOLBOX_PROFILER_HPP
