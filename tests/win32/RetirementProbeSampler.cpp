#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <cwchar>
#include <string>
#include <cstdio>
#include <new>

#include "RetirementProbeSampler.hpp"
#include "core/Operation.hpp"
#include "platform/Win32String.hpp"
#include "platform/file/FileIO.hpp"
#include "testing/app/NativeResourceRetirementTestAccess.hpp"

/** Completed process/ledger observation; no resource ownership. */
struct RetirementProbeSample
{
  unsigned long gdi, user;
  double privateBytes;
  std::size_t held, queued, inFlight;
  bool active;
};

struct RetirementProbeLog::Impl
{
  Impl() : file_(), stream_(0), healthy_(false), phase_(0), samples_(0), base_(), peak_(), final_() {}
  loka::platform::file::FileHandle file_;
  std::FILE *stream_;
  bool healthy_;
  const char *phase_;
  unsigned samples_;
  RetirementProbeSample base_, peak_, final_;
};

namespace
{
  bool TakeSample(const PlatformContext &context, RetirementProbeSample &out)
  {
    PROCESS_MEMORY_COUNTERS_EX memory = {};
    memory.cb = static_cast<DWORD>(sizeof(memory));
    if (!GetProcessMemoryInfo(GetCurrentProcess(),
          reinterpret_cast<PROCESS_MEMORY_COUNTERS *>(&memory), memory.cb))
      return false;
    typedef loka::app::testing::NativeResourceRetirementTestAccess Access;
    out.gdi = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    out.user = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    out.privateBytes = static_cast<double>(memory.PrivateUsage);
    out.held = Access::held(context);
    out.queued = Access::queued(context);
    out.inFlight = Access::inFlight(context);
    out.active = loka::core::Operation::hasActive();
    return true;
  }
}

bool ResolveRetirementProbeFile(const wchar_t *name,
                                loka::platform::file::FileHandle &file, std::wstring &path)
{
  wchar_t module[32768];
  const DWORD length = GetModuleFileNameW(0, module, 32768);
  if (!length || length >= 32768)
    return false;
  path.assign(module, length);
  const std::wstring::size_type slash = path.find_last_of(L"\\/");
  if (slash == std::wstring::npos)
    return false;
  path.erase(slash + 1);
  path += name;
  file.displayPath = loka::core::String(loka::win32::CreateWin32StringFromUtf16(path.c_str(), path.size()));
  file.kind = loka::file::File::KIND_FILE;
  return true;
}

RetirementProbeLog::RetirementProbeLog(const wchar_t *name)
    : impl_(new (std::nothrow) Impl)
{
  if (!this->impl_) return;
  std::wstring path;
  if (!ResolveRetirementProbeFile(name, this->impl_->file_, path)) return;
  this->impl_->stream_ = loka::platform::file::OpenWriteTruncate(this->impl_->file_);
  this->impl_->healthy_ = this->impl_->stream_ != 0;
}

void RetirementProbeLog::note(const char *text)
{
  if (!this->valid()) return;
  std::fprintf(this->impl_->stream_, "%s\n", text);
  this->flush();
}

RetirementProbeLog::~RetirementProbeLog()
{
  if (!this->impl_) return;
  if (this->impl_->stream_)
    std::fclose(this->impl_->stream_);
  delete this->impl_;
}

bool RetirementProbeLog::valid() const { return this->impl_ && this->impl_->healthy_; }

void RetirementProbeLog::begin(const char *phase)
{
  if (!this->impl_) return;
  this->impl_->phase_ = phase;
  this->impl_->samples_ = 0;
}

void RetirementProbeLog::flush()
{
  if (!this->impl_->stream_ || std::ferror(this->impl_->stream_)
      || !loka::platform::file::FlushWrite(this->impl_->stream_, this->impl_->file_))
    this->impl_->healthy_ = false;
}

void RetirementProbeLog::error(const char *reason)
{
  if (this->impl_ && this->impl_->stream_)
  {
    std::fprintf(this->impl_->stream_, "error=%s\n", reason);
    this->flush();
  }
  if (this->impl_) this->impl_->healthy_ = false;
}

void RetirementProbeLog::sample(const PlatformContext &context, int step, const char *point,
                                int page, int width, int height)
{
  if (!this->valid() || !this->impl_->phase_)
    return;
  RetirementProbeSample value;
  if (!TakeSample(context, value))
  {
    this->error("process-memory-query-failed");
    return;
  }
  if (this->impl_->samples_++ == 0)
    this->impl_->base_ = this->impl_->peak_ = value;
  if (value.gdi > this->impl_->peak_.gdi) this->impl_->peak_.gdi = value.gdi;
  if (value.privateBytes > this->impl_->peak_.privateBytes) this->impl_->peak_.privateBytes = value.privateBytes;
  if (value.queued > this->impl_->peak_.queued) this->impl_->peak_.queued = value.queued;
  this->impl_->final_ = value;
  std::fprintf(this->impl_->stream_,
      "phase=%s step=%d point=%s page=%d gdi=%lu user=%lu private=%.0f held=%lu queued=%lu inflight=%lu active=%d",
      this->impl_->phase_, step, point, page, value.gdi, value.user, value.privateBytes,
      static_cast<unsigned long>(value.held), static_cast<unsigned long>(value.queued),
      static_cast<unsigned long>(value.inFlight), value.active ? 1 : 0);
  if (width > 0 && height > 0)
    std::fprintf(this->impl_->stream_, " img=%dx%d", width, height);
  std::fprintf(this->impl_->stream_, "\n");
  this->flush();
}

void RetirementProbeLog::summary()
{
  if (!this->valid() || !this->impl_->samples_)
    return;
  std::fprintf(this->impl_->stream_,
      "summary phase=%s base_gdi=%lu peak_gdi=%lu final_gdi=%lu base_private=%.0f peak_private=%.0f final_private=%.0f peak_queued=%lu final_queued=%lu\n",
      this->impl_->phase_, this->impl_->base_.gdi, this->impl_->peak_.gdi, this->impl_->final_.gdi,
      this->impl_->base_.privateBytes, this->impl_->peak_.privateBytes, this->impl_->final_.privateBytes,
      static_cast<unsigned long>(this->impl_->peak_.queued), static_cast<unsigned long>(this->impl_->final_.queued));
  this->flush();
}
