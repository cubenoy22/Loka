#ifndef TOOLBOX_PROFILER_HPP
#define TOOLBOX_PROFILER_HPP

#include "core/Profiler.hpp"
#include <string>

// Re-export common profiler types for convenience
using declara::core::ProfileEntry;
using declara::core::ScopedProfile;
using declara::core::gProfileData;
using declara::core::gProfileCount;

// Toolbox-specific: result string for display after BuildProfileResultString
extern std::string gProfileResultString;

// Initialize the Toolbox profiler backend (call at startup)
void InitToolboxProfiler();

// Build result string from profile data and reset count
void BuildProfileResultString();

#endif // TOOLBOX_PROFILER_HPP
