#include "StandalonePerformance.hpp"

#include <climits>
#include <cstdio>

#include "core/io/File.hpp"
#include "platform/file/AppLocation.hpp"
#include "platform/file/FileIO.hpp"
#include "testing/snap/SnapFormat.hpp"

namespace loka
{
  namespace standalone_tests
  {
    namespace
    {
      void AppendLong(std::string &out, const char *name, long value)
      {
        char buffer[96];
        const int length = ::snprintf(buffer, sizeof(buffer), "%s=%ld\n", name, value);
        if (length > 0 && static_cast<std::size_t>(length) < sizeof(buffer))
        {
          out.append(buffer, static_cast<std::size_t>(length));
        }
      }

      void AppendRunLong(std::string &out, long run, long value)
      {
        char buffer[96];
        const int length = ::snprintf(buffer, sizeof(buffer), "run.%ld.elapsed_ticks=%ld\n", run, value);
        if (length > 0 && static_cast<std::size_t>(length) < sizeof(buffer))
        {
          out.append(buffer, static_cast<std::size_t>(length));
        }
      }

      void Summarize(const long *values, long count, long &minimum, long &maximum, long &mean)
      {
        minimum = values[0];
        maximum = values[0];
        mean = 0;
        long remainder = 0;
        for (long i = 0; i < count; ++i)
        {
          if (values[i] < minimum)
          {
            minimum = values[i];
          }
          if (values[i] > maximum)
          {
            maximum = values[i];
          }
          mean += values[i] / count;
          remainder += values[i] % count;
        }
        mean += remainder / count;
      }

      bool CloseWrite(std::FILE *output, const platform::file::FileHandle &file, bool contentReady)
      {
        const bool flushed = contentReady && platform::file::FlushWrite(output, file);
        const bool closed = std::fclose(output) == 0;
        return flushed && closed;
      }
    } // namespace

    StandaloneScenarioVerdict::StandaloneScenarioVerdict()
        : state_(STATE_NOT_STARTED)
    {
    }

    void StandaloneScenarioVerdict::begin()
    {
      if (this->state_ == STATE_NOT_STARTED)
      {
        this->state_ = STATE_AWAITING_TERMINAL;
      }
    }

    void StandaloneScenarioVerdict::observe(const dsl::SnapRecord &record)
    {
      if (this->state_ != STATE_AWAITING_TERMINAL)
      {
        return;
      }
      std::string status;
      this->state_ = record.get("status", status) && status == dsl::SnapStatusOk() ? STATE_SUCCEEDED : STATE_FAILED;
    }

    bool StandaloneScenarioVerdict::refusesCompletedPass() const
    {
      return this->state_ == STATE_AWAITING_TERMINAL || this->state_ == STATE_FAILED;
    }

    StandalonePerformanceSession::StandalonePerformanceSession(long runCount)
        : runCount_(runCount),
          valid_(runCount >= MIN_RUN_COUNT && runCount <= MAX_RUN_COUNT),
          completedRuns_(0)
    {
      for (long i = 0; i < MAX_RUN_COUNT; ++i)
      {
        this->elapsedByRun_[i] = 0;
      }
    }

    bool StandalonePerformanceSession::isValid() const
    {
      return this->valid_;
    }

    bool StandalonePerformanceSession::isComplete() const
    {
      return this->valid_ && this->completedRuns_ == this->runCount_;
    }

    bool StandalonePerformanceSession::recordRun(long startTicks, long endTicks)
    {
      const unsigned long elapsed = static_cast<unsigned long>(endTicks) - static_cast<unsigned long>(startTicks);
      if (!this->valid_ || this->isComplete() || elapsed == 0 || elapsed > static_cast<unsigned long>(LONG_MAX))
      {
        this->valid_ = false;
        return false;
      }
      this->elapsedByRun_[this->completedRuns_] = static_cast<long>(elapsed);
      ++this->completedRuns_;
      return true;
    }

    bool StandalonePerformanceSession::buildReport(std::string &out) const
    {
      if (!this->isComplete())
      {
        return false;
      }

      std::string completed;
      completed.reserve(768);
      completed += "performance_version=1\n";
      completed += "clock=platform_profile_ticks\n";
      completed += "scope=standalone_flow_application\n";
      AppendLong(completed, "run_count", this->runCount_);
      for (long i = 0; i < this->runCount_; ++i)
      {
        AppendRunLong(completed, i + 1, this->elapsedByRun_[i]);
      }

      long minimum = 0;
      long maximum = 0;
      long mean = 0;
      Summarize(this->elapsedByRun_, this->runCount_, minimum, maximum, mean);
      AppendLong(completed, "summary.elapsed_ticks.min", minimum);
      AppendLong(completed, "summary.elapsed_ticks.max", maximum);
      AppendLong(completed, "summary.elapsed_ticks.mean", mean);
      out = completed;
      return true;
    }

    bool ResolveStandalonePerformanceReport(platform::file::FileHandle &out)
    {
      return platform::file::ResolveApplicationSidecar(file::File::Application() << file::File("PERF.TXT"), out);
    }

    bool PrepareStandalonePerformanceReport(const platform::file::FileHandle &file)
    {
      std::FILE *output = platform::file::OpenWriteTruncate(file);
      return output && CloseWrite(output, file, true);
    }

    bool WriteStandalonePerformanceReport(const platform::file::FileHandle &file,
                                          const StandalonePerformanceSession &session)
    {
      std::string report;
      if (!session.buildReport(report))
      {
        return false;
      }
      std::FILE *output = platform::file::OpenWriteTruncate(file);
      if (!output)
      {
        return false;
      }
      const bool wrote = std::fwrite(report.c_str(), 1, report.size(), output) == report.size();
      return CloseWrite(output, file, wrote);
    }
  } // namespace standalone_tests
} // namespace loka
