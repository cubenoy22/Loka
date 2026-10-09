#ifndef LOKA_TESTS_WIN32_RETIREMENT_PROBE_SAMPLER_HPP
#define LOKA_TESTS_WIN32_RETIREMENT_PROBE_SAMPLER_HPP

#include <cstdio>
#include <string>
#include "app/PlatformContext.hpp"
#include "platform/file/FileHandle.hpp"

/** Resolves a wide executable-sidecar path; shared by logs and input fixtures. */
bool ResolveRetirementProbeFile(const wchar_t *name,
                                loka::platform::file::FileHandle &file, std::wstring &path);

/** Completed process/ledger observation; no resource ownership. */
struct RetirementProbeSample
{
  unsigned long gdi, user;
  double privateBytes;
  std::size_t held, queued, inFlight;
  bool active;
};

/** Owns the sidecar stream and the current phase's measurement aggregate. */
class RetirementProbeLog
{
public:
  explicit RetirementProbeLog(const wchar_t *name);
  ~RetirementProbeLog();
  bool valid() const;
  void begin(const char *phase);
  void sample(const PlatformContext &context, int step, const char *point,
              int page = 0, int width = 0, int height = 0);
  void summary();
  void note(const char *text);
  void error(const char *reason);

private:
  RetirementProbeLog(const RetirementProbeLog &);
  RetirementProbeLog &operator=(const RetirementProbeLog &);
  void flush();
  loka::platform::file::FileHandle file_;
  std::FILE *stream_;
  bool healthy_;
  const char *phase_;
  unsigned int samples_;
  RetirementProbeSample base_, peak_, final_;
};

#endif
