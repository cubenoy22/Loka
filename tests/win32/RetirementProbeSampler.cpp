#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <cwchar>
#include <string>

#include "RetirementProbeSampler.hpp"
#include "core/Operation.hpp"
#include "platform/Win32String.hpp"
#include "platform/file/FileIO.hpp"
#include "testing/app/NativeResourceRetirementTestAccess.hpp"

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
    : file_(), stream_(0), healthy_(false), phase_(0), samples_(0), base_(), peak_(), final_()
{
  std::wstring path;
  if (!ResolveRetirementProbeFile(name, this->file_, path)) return;
  this->stream_ = loka::platform::file::OpenWriteTruncate(this->file_);
  this->healthy_ = this->stream_ != 0;
}

void RetirementProbeLog::note(const char *text)
{
  if (!this->valid()) return;
  std::fprintf(this->stream_, "%s\n", text);
  this->flush();
}

RetirementProbeLog::~RetirementProbeLog()
{
  if (this->stream_)
    std::fclose(this->stream_);
}

bool RetirementProbeLog::valid() const { return this->healthy_; }

void RetirementProbeLog::begin(const char *phase)
{
  this->phase_ = phase;
  this->samples_ = 0;
}

void RetirementProbeLog::flush()
{
  if (!this->stream_ || std::ferror(this->stream_)
      || !loka::platform::file::FlushWrite(this->stream_, this->file_))
    this->healthy_ = false;
}

void RetirementProbeLog::error(const char *reason)
{
  if (this->stream_)
  {
    std::fprintf(this->stream_, "error=%s\n", reason);
    this->flush();
  }
  this->healthy_ = false;
}

void RetirementProbeLog::sample(const PlatformContext &context, int step, const char *point,
                                int page, int width, int height)
{
  if (!this->valid() || !this->phase_)
    return;
  RetirementProbeSample value;
  if (!TakeSample(context, value))
  {
    this->error("process-memory-query-failed");
    return;
  }
  if (this->samples_++ == 0)
    this->base_ = this->peak_ = value;
  if (value.gdi > this->peak_.gdi) this->peak_.gdi = value.gdi;
  if (value.privateBytes > this->peak_.privateBytes) this->peak_.privateBytes = value.privateBytes;
  if (value.queued > this->peak_.queued) this->peak_.queued = value.queued;
  this->final_ = value;
  std::fprintf(this->stream_,
      "phase=%s step=%d point=%s page=%d gdi=%lu user=%lu private=%.0f held=%lu queued=%lu inflight=%lu active=%d",
      this->phase_, step, point, page, value.gdi, value.user, value.privateBytes,
      static_cast<unsigned long>(value.held), static_cast<unsigned long>(value.queued),
      static_cast<unsigned long>(value.inFlight), value.active ? 1 : 0);
  if (width > 0 && height > 0)
    std::fprintf(this->stream_, " img=%dx%d", width, height);
  std::fprintf(this->stream_, "\n");
  this->flush();
}

void RetirementProbeLog::summary()
{
  if (!this->valid() || !this->samples_)
    return;
  std::fprintf(this->stream_,
      "summary phase=%s base_gdi=%lu peak_gdi=%lu final_gdi=%lu base_private=%.0f peak_private=%.0f final_private=%.0f peak_queued=%lu final_queued=%lu\n",
      this->phase_, this->base_.gdi, this->peak_.gdi, this->final_.gdi,
      this->base_.privateBytes, this->peak_.privateBytes, this->final_.privateBytes,
      static_cast<unsigned long>(this->peak_.queued), static_cast<unsigned long>(this->final_.queued));
  this->flush();
}
