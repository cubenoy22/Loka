#include <cstdio>
#include <Events.h>
#include <Memory.h>
#include <Processes.h>
#include <Timer.h>

#include "StandaloneFlowRunner.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/style/AttributedString.hpp"
#include "core/LokaAlloc.hpp"
#include "core/State.hpp"
#include "core/io/File.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "platform/file/AppLocation.hpp"
#include "platform/file/FileIO.hpp"

#if !defined(LOKA_RETRO68) || !defined(TEST_BUILD)
#error Attributed string probe requires a Toolbox test build
#endif

namespace
{
  using namespace loka::app;
  using namespace loka::core;
  const int kLines = 100;
  const int kIterations = 1000;

  /** Stack timer: unsigned subtraction handles a low-word wrap (<71 minutes). */
  struct Timer
  {
    const unsigned long ticks;
    UnsignedWide start;
    Timer()
        : ticks(TickCount())
    {
      Microseconds(&this->start);
    }
    unsigned long elapsed() const
    {
      UnsignedWide end;
      Microseconds(&end);
      return end.lo - this->start.lo;
    }
  };

  /** Heap queries precede compaction and all report I/O, as in AppRecycle. */
  struct Sample
  {
    const unsigned long us;
    const unsigned long ticks;
    const long freeBytes;
    const long maxBlock;
    const long compactBlock;
    explicit Sample(const Timer &timer)
        : us(timer.elapsed()),
          ticks(TickCount() - timer.ticks),
          freeBytes(FreeMem()),
          maxBlock(MaxBlock()),
          compactBlock(CompactMem(0x7FFFFFFFL))
    {
    }
    void write(std::FILE *log, const char *phase) const
    {
      std::fprintf(log,
                   "phase=%s FreeMem=%ld MaxBlock=%ld CompactMem=%ld us=%lu ticks=%lu",
                   phase,
                   this->freeBytes,
                   this->maxBlock,
                   this->compactBlock,
                   this->us,
                   this->ticks);
    }
  };

  AttributedString BuildLine(int index, int version)
  {
    char name[7];
    char number[7];
    std::sprintf(name, "v%02d", index);
    std::sprintf(number, "%03d", version);
    // Each tokenizer output String is constructed exactly once per line.
    const String text[10] = {String::Literal("var "),
                             String::Literal(name),
                             String::Literal(" = "),
                             String::Literal(number),
                             String::Literal(" + "),
                             String::Literal("foo"),
                             String::Literal(" * "),
                             String::Literal("02"),
                             String::Literal("; "),
                             String::Literal("//ok")};
    return Styled(text[0], Bold) + Styled(text[1], Italic) + Styled(text[2], TextStyle())
           + Styled(text[3], FontSize<12>()) + Styled(text[4], TextStyle()) + Styled(text[5], Italic)
           + Styled(text[6], TextStyle()) + Styled(text[7], FontSize<12>()) + Styled(text[8], TextStyle())
           + Styled(text[9], Bold);
  }

  /** App-config-owned fixed states; tracker dies before its registered states. */
  class ProbeConfig : public AppConfigurable
  {
  public:
    explicit ProbeConfig(PlatformContext *context)
        : AppConfigurable(context),
          app_(0),
          log_(0),
          result_(0)
    {
      this->tracker_.reserveStates(kLines);
      for (int i = 0; i < kLines; ++i)
        this->tracker_.addState(&this->lines_[i]);
      if (loka::platform::file::ResolveApplicationSidecar(
              loka::file::File::Application() << loka::file::File("LOG.TXT"), this->file_))
        this->log_ = loka::platform::file::OpenWriteTruncate(this->file_);
      if (this->log_)
        std::setvbuf(this->log_, 0, _IONBF, 0);
      else
        this->result_ = 1;
    }
    virtual ~ProbeConfig()
    {
      if (this->log_)
        std::fclose(this->log_);
    }
    void setApp(App *app)
    {
      this->app_ = app;
    }
    int exitCode() const
    {
      return this->result_;
    }
    virtual void compose(AppComposition &composition)
    {
      composition << WindowDefinition<WindowProps>(WindowProps()
                                                       .title("AttributedString probe")
                                                       .visible(true)
                                                       .idlePolicy(IdlePolicy::everyTick())
                                                       .onIdle(&ProbeConfig::OnIdle, this));
    }

  private:
    App *app_;
    loka::platform::file::FileHandle file_;
    std::FILE *log_;
    int result_;
    MutableState<AttributedString> lines_[kLines];
    PushStateTracker tracker_;

    bool store(int index, int version)
    {
      const AttributedString value = BuildLine(index, version);
      if (!value.valid() || value.segmentCount() != 10)
        return false;
      {
        StateTrackerGuard transaction(&this->tracker_);
        this->lines_[index].set(value);
      }
      // Positive control for P3's no-dirty observation: P1/P2 must change.
      return this->tracker_.transactionDirty();
    }

    bool measure()
    {
      const Sample p0((Timer()));
      p0.write(this->log_, "P0-baseline");
      std::fprintf(this->log_, "\r");
      const Timer build;
      for (int i = 0; i < kLines; ++i)
        if (!this->store(i, 0))
          return false;
      const Sample p1(build);
      p1.write(this->log_, "P1-build");
      const long bytes = p0.freeBytes - p1.freeBytes;
      std::fprintf(this->log_,
                   " heap_bytes=%ld bytes_per_line=%ld bytes_per_segment=%ld lines=100 segments=1000\r",
                   bytes,
                   bytes / kLines,
                   bytes / (kLines * 10));

      const Timer churn;
      for (int i = 0; i < kIterations; ++i)
        if (!this->store(i % kLines, i / kLines + 1))
          return false;
      const Sample p2(churn);
      p2.write(this->log_, "P2-churn");
      std::fprintf(this->log_,
                   " us_per_keystroke=%lu FreeMem_before=%ld FreeMem_after=%ld growth=%ld\r",
                   p2.us / kIterations,
                   p1.freeBytes,
                   p2.freeBytes,
                   p1.freeBytes - p2.freeBytes);

      unsigned long setUs = 0;
      int dirty = 0;
      const Timer equal;
      for (int i = 0; i < kIterations; ++i)
      {
        const AttributedString value = BuildLine(0, 10);
        if (!value.valid() || value.segmentCount() != 10)
          return false;
        {
          StateTrackerGuard transaction(&this->tracker_);
          const Timer compare;
          this->lines_[0].set(value);
          setUs += compare.elapsed();
        }
        if (this->tracker_.transactionDirty())
          ++dirty;
      }
      const Sample p3(equal);
      p3.write(this->log_, "P3-equal-rebuild");
      std::fprintf(this->log_,
                   " us_per_rebuild=%lu set_us=%lu set_us_per_rebuild=%lu dirty=%d\r",
                   p3.us / kIterations,
                   setUs,
                   setUs / kIterations,
                   dirty);

      int matches = 0;
      const Timer fast;
      {
        const AttributedString left = this->lines_[0].get();
        const AttributedString right = left;
        for (int i = 0; i < kIterations; ++i)
          if (left == right)
            ++matches;
      }
      const Sample p4(fast);
      p4.write(this->log_, "P4-shared-compare");
      std::fprintf(this->log_, " us_per_compare=%lu matches=%d\r", p4.us / kIterations, matches);

      const Timer teardown;
      {
        StateTrackerGuard transaction(&this->tracker_);
        for (int i = 0; i < kLines; ++i)
          this->lines_[i].set(AttributedString());
      }
      const Sample p5(teardown);
      p5.write(this->log_, "P5-teardown");
      std::fprintf(this->log_, " delta_from_P0=%ld\r", p0.freeBytes - p5.freeBytes);
#if defined(LOKA_DIAG) || defined(LOKA_RETRO68_DIAGNOSTICS)
      LokaAllocCensusDump(this->log_);
#else
      std::fprintf(this->log_, "census=unavailable\r");
#endif
      return dirty == 0 && matches == kIterations;
    }

    static void OnIdle(Window *, double, void *data)
    {
      ProbeConfig *self = static_cast<ProbeConfig *>(data);
      if (!self->measure())
      {
        self->result_ = 1;
        std::fprintf(self->log_, "ERROR allocation/segment/equality check failed\r");
      }
      std::fprintf(self->log_, "done\r");
      if (std::ferror(self->log_) || !loka::platform::file::FlushWrite(self->log_, self->file_))
        self->result_ = 1;
      self->app_->quit();
    }
  };
} // namespace

int main(int, char **)
{
  const int result = loka::standalone_tests::RunStandaloneFlowWithConfig<ProbeConfig>(0, 0);
  ExitToShell();
  return result;
}
