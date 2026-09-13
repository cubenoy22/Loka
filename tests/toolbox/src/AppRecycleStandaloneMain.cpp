#include <cstdio>

#include "HelloWorldStandaloneFlowAppConfig.hpp"
#include "StandaloneFlowRunner.hpp"
#include "core/io/File.hpp"
#include "platform/file/AppLocation.hpp"
#include "platform/file/FileIO.hpp"

#include <Events.h>
#include <Memory.h>
#include <Processes.h>

#if !defined(LOKA_RETRO68) || !defined(TEST_BUILD)
#error App recycle probe requires a Toolbox test build
#endif

namespace
{
  const int kPasses = loka::standalone_tests::kConfiguredPerformanceRuns;
  const long kAllowance = 1024;

  /** Completed heap observation; queries precede compaction and log I/O. */
  struct HeapSample
  {
    const long freeBytes;
    const long maxBlock;
    const long compactBlock;

    HeapSample()
        : freeBytes(FreeMem()),
          maxBlock(MaxBlock()),
          compactBlock(CompactMem(0x7FFFFFFFL))
    {
    }
  };

  bool WriteSample(std::FILE *log,
                   const loka::platform::file::FileHandle &file,
                   int pass,
                   const HeapSample &sample,
                   unsigned long ticks)
  {
    return std::fprintf(log,
                        "pass=%d FreeMem=%ld MaxBlock=%ld CompactMem=%ld ticks=%lu\r",
                        pass,
                        sample.freeBytes,
                        sample.maxBlock,
                        sample.compactBlock,
                        ticks)
               >= 0
           && loka::platform::file::FlushWrite(log, file);
  }

  // Mirrors StandaloneFlowRunner::RunPass: App dies before Config, both before
  // the caller samples. A separate audit file prevents truncating LOG.TXT.
  int RunPass(PlatformContext *context, const loka::platform::file::FileHandle &auditFile)
  {
    loka::standalone_tests::HelloWorldStandaloneFlowAppConfig config(context, &auditFile);
    if (config.exitCode() != 0)
      return config.exitCode();
    loka::core::ScopedPtr<App> app(context->createApp(&config, 0, 0));
    if (!app.get())
      return 1;
    config.setApp(app.get());
    app->run();
    return config.exitCode();
  }

  int Measure(std::FILE *log,
              const loka::platform::file::FileHandle &file,
              const loka::platform::file::FileHandle &auditFile)
  {
    loka::platform::InitPlatformRuntime();
    loka::core::ScopedPtr<PlatformContext> context(loka::platform::CreatePlatformContext());
    if (!context.get())
      return 1;

    // Keep completed samples on the stack; no measurement allocations per pass.
    const HeapSample before;
    if (!WriteSample(log, file, 0, before, 0))
      return 1;
    long baselineFree = 0;
    long baselineBlock = 0;
    long baselineCompact = 0;
    for (int pass = 1; pass <= kPasses; ++pass)
    {
      const unsigned long start = TickCount();
      const int result = RunPass(context.get(), auditFile);
      const unsigned long ticks = TickCount() - start;
      const HeapSample sample;
      if (!WriteSample(log, file, pass, sample, ticks))
        return 1;
      if (result != 0)
      {
        std::fprintf(log, "app-recycle ERROR pass=%d code=%d\r", pass, result);
        return 1;
      }
      if (pass == 2)
      {
        baselineFree = sample.freeBytes;
        baselineBlock = sample.maxBlock;
        baselineCompact = sample.compactBlock;
      }
      if (pass == kPasses)
      {
        const long freeLoss = baselineFree - sample.freeBytes;
        const long blockLoss = baselineBlock - sample.maxBlock;
        const long compactLoss = baselineCompact - sample.compactBlock;
        long growth = freeLoss > blockLoss ? freeLoss : blockLoss;
        if (compactLoss > growth)
          growth = compactLoss;
        // More available memory is not growth. Report the larger loss in bytes.
        if (std::fprintf(log, "app-recycle DONE\r") < 0)
          return 1;
        if (growth <= kAllowance)
          return std::fprintf(log, "PASS\r") >= 0 ? 0 : 1;
        return std::fprintf(log, "GROWTH %ld\r", growth) >= 0 ? 0 : 1;
      }
    }
    return 1;
  }

  int RunProbe()
  {
    loka::platform::file::FileHandle file;
    loka::platform::file::FileHandle auditFile;
    if (!loka::platform::file::ResolveApplicationSidecar(loka::file::File::Application() << loka::file::File("LOG.TXT"),
                                                         file)
        || !loka::platform::file::ResolveApplicationSidecar(
            loka::file::File::Application() << loka::file::File("AUDIT.TXT"), auditFile))
      return 1;
    std::FILE *log = loka::platform::file::OpenWriteTruncate(file);
    if (!log)
      return 1;
    // Avoid a lazy stdio buffer allocation between the baseline and pass 1.
    std::setvbuf(log, 0, _IONBF, 0);
    const int result = Measure(log, file, auditFile);
    const bool flushed = loka::platform::file::FlushWrite(log, file);
    const int closed = std::fclose(log);
    return result == 0 && flushed && closed == 0 ? 0 : 1;
  }
} // namespace

int main(int, char **)
{
  const int result = RunProbe();
  ExitToShell();
  return result;
}
