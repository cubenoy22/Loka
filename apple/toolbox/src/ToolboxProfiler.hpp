#ifndef TOOLBOX_PROFILER_HPP
#define TOOLBOX_PROFILER_HPP

#include "core/Profiler.hpp"
#include <string>

// Re-export common profiler types for convenience
using loka::core::ProfileEntry;
using loka::core::ScopedProfile;
using loka::core::gProfileData;
using loka::core::gProfileCount;

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
