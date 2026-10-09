#include "RetirementProbeLog.hpp"
#include <Foundation/Foundation.h>
#include <mach/mach.h>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <new>
#include "core/Operation.hpp"
#include "testing/app/NativeResourceRetirementTestAccess.hpp"

struct RetirementProbeLog::Impl
{
  struct Sample { double rss, footprint; size_t held, queued, inFlight; bool active; };
  Impl() : stream(0), healthy(false), phase(0), samples(0), base(), peak(), final() {}
  FILE *stream;
  bool healthy;
  const char *phase;
  unsigned samples;
  Sample base, peak, final;
};

RetirementProbeLog::RetirementProbeLog(const wchar_t *name) : impl_(new (std::nothrow) Impl)
{
  if (!this->impl_) return;
  char filename[256];
  if (std::wcstombs(filename, name, sizeof(filename)) >= sizeof(filename)) return;
  const char *directory = std::getenv("LOKA_RETIREMENT_PROBE_OUTPUT_DIR");
  NSString *dir = directory ? [NSString stringWithUTF8String:directory]
      : [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
  NSString *path = [dir stringByAppendingPathComponent:[NSString stringWithUTF8String:filename]];
  this->impl_->stream = std::fopen([path fileSystemRepresentation], "w");
  this->impl_->healthy = this->impl_->stream != 0;
  if (!this->impl_->healthy) std::fprintf(stderr, "error=log-open-failed\n");
}
RetirementProbeLog::~RetirementProbeLog()
{
  if (!this->impl_) return;
  if (this->impl_->stream) std::fclose(this->impl_->stream);
  delete this->impl_;
}
bool RetirementProbeLog::valid() const { return this->impl_ && this->impl_->healthy; }
void RetirementProbeLog::flush()
{
  if (std::fflush(this->impl_->stream) || std::ferror(this->impl_->stream)) this->impl_->healthy = false;
}
void RetirementProbeLog::begin(const char *phase)
{
  if (!this->impl_) return;
  this->impl_->phase = phase;
  this->impl_->samples = 0;
}
void RetirementProbeLog::note(const char *text)
{
  if (!this->valid()) return;
  std::fprintf(this->impl_->stream, "%s\n", text);
  this->flush();
}
void RetirementProbeLog::error(const char *reason)
{
  if (this->valid()) { std::fprintf(this->impl_->stream, "error=%s\n", reason); this->flush(); }
  if (this->impl_) this->impl_->healthy = false;
}
void RetirementProbeLog::sample(const PlatformContext &context, int step, const char *point,
                                int page, int width, int height)
{
  if (!this->valid() || !this->impl_->phase) return;
  mach_task_basic_info_data_t basic;
  task_vm_info_data_t vm;
  mach_msg_type_number_t basicCount = MACH_TASK_BASIC_INFO_COUNT, vmCount = TASK_VM_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&basic), &basicCount) != KERN_SUCCESS
      || task_info(mach_task_self(), TASK_VM_INFO, reinterpret_cast<task_info_t>(&vm), &vmCount) != KERN_SUCCESS)
  { this->error("process-memory-query-failed"); return; }
  typedef loka::app::testing::NativeResourceRetirementTestAccess Access;
  Impl::Sample value = {static_cast<double>(basic.resident_size), static_cast<double>(vm.phys_footprint),
      Access::held(context), Access::queued(context), Access::inFlight(context), loka::core::Operation::hasActive()};
  Impl &s = *this->impl_;
  if (s.samples++ == 0) s.base = s.peak = value;
  if (value.rss > s.peak.rss) s.peak.rss = value.rss;
  if (value.footprint > s.peak.footprint) s.peak.footprint = value.footprint;
  if (value.queued > s.peak.queued) s.peak.queued = value.queued;
  s.final = value;
  std::fprintf(s.stream, "phase=%s step=%d point=%s page=%d rss=%.0f footprint=%.0f held=%lu queued=%lu inflight=%lu active=%d",
      s.phase, step, point, page, value.rss, value.footprint, static_cast<unsigned long>(value.held),
      static_cast<unsigned long>(value.queued), static_cast<unsigned long>(value.inFlight), value.active ? 1 : 0);
  if (width > 0 && height > 0) std::fprintf(s.stream, " img=%dx%d", width, height);
  std::fprintf(s.stream, "\n"); this->flush();
}
void RetirementProbeLog::summary()
{
  if (!this->valid() || !this->impl_->samples) return;
  Impl &s = *this->impl_;
  std::fprintf(s.stream, "summary phase=%s base_rss=%.0f peak_rss=%.0f final_rss=%.0f base_footprint=%.0f peak_footprint=%.0f final_footprint=%.0f peak_queued=%lu final_queued=%lu\n",
      s.phase, s.base.rss, s.peak.rss, s.final.rss, s.base.footprint, s.peak.footprint, s.final.footprint,
      static_cast<unsigned long>(s.peak.queued), static_cast<unsigned long>(s.final.queued));
  this->flush();
}
