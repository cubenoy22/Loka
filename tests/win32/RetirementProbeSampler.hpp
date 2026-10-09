#ifndef LOKA_WIN32_RETIREMENT_PROBE_SAMPLER_HPP
#define LOKA_WIN32_RETIREMENT_PROBE_SAMPLER_HPP
#include <string>
#include "RetirementProbeLog.hpp"
#include "platform/file/FileHandle.hpp"
bool ResolveRetirementProbeFile(const wchar_t *name, loka::platform::file::FileHandle &file, std::wstring &path);
#endif
