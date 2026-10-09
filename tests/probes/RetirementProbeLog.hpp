#ifndef LOKA_RETIREMENT_PROBE_LOG_HPP
#define LOKA_RETIREMENT_PROBE_LOG_HPP
#include "app/PlatformContext.hpp"
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
  struct Impl;
  Impl *impl_;
};

#endif
