#ifndef LOKA_DSL_SNAP_FLOW_HPP
#define LOKA_DSL_SNAP_FLOW_HPP

#include <string>
#include "loka/dsl/Flow.hpp"
#include "loka/dsl/SnapFormat.hpp"

namespace loka
{
  namespace dsl
  {
    enum SnapFlowErrorKind
    {
      FLOW_ERROR_KIND_SNAP = 1001
    };

    enum SnapFlowErrorCode
    {
      FLOW_ERROR_SNAP_MISSING_REQUIRED_KEY = 1,
      FLOW_ERROR_SNAP_WRITE_FAILED = 2
    };

    class SnapWriteAdapter
    {
    public:
      typedef SnapRecord In;
      typedef SnapRecord Out;

      explicit SnapWriteAdapter(const char *path,
                                bool requireV1 = true,
                                const char *defaultNodeId = 0,
                                const char *configPath = "LokaTest.cfg")
          : path_(path ? path : ""),
            requireV1_(requireV1),
            defaultNodeId_(defaultNodeId ? defaultNodeId : ""),
            configPath_(configPath ? configPath : "") {}

      StepRunStatus run(const In &in, Out &out, FlowError &error) const
      {
        out = in;
        if (!defaultNodeId_.empty() && !out.has("node"))
        {
          out.set("node", defaultNodeId_.c_str());
        }
        if (requireV1_)
        {
          std::string missingKey;
          if (!out.validateV1RequiredKeys(missingKey))
          {
            (void)missingKey;
            error.kind = FLOW_ERROR_KIND_SNAP;
            error.code = FLOW_ERROR_SNAP_MISSING_REQUIRED_KEY;
            return FLOW_STEP_FAILED;
          }
        }
        const std::string outputPath = SnapTestConfig::resolveCapturePath(path_.c_str(), configPath_.c_str());
        SnapTestConfig::Settings settings;
        const bool hasConfig = SnapTestConfig::load(configPath_.c_str(), settings);
        bool writeOk = false;
        if (hasConfig && (settings.hasMaxTotalBytes || settings.hasMaxFiles))
        {
          writeOk = SnapFileWriter::appendRecordWithLimits(outputPath.c_str(),
                                                           out,
                                                           settings.hasMaxTotalBytes ? settings.maxTotalBytes : 0,
                                                           settings.hasMaxFiles ? settings.maxFiles : 0);
        }
        else
        {
          writeOk = SnapFileWriter::appendRecord(outputPath.c_str(), out);
        }
        if (!writeOk)
        {
          error.kind = FLOW_ERROR_KIND_SNAP;
          error.code = FLOW_ERROR_SNAP_WRITE_FAILED;
          return FLOW_STEP_FAILED;
        }
        return FLOW_STEP_SUCCEEDED;
      }

    private:
      std::string path_;
      bool requireV1_;
      std::string defaultNodeId_;
      std::string configPath_;
    };

    class BuildSnapV1RecordAdapter
    {
    public:
      typedef int In;
      typedef SnapRecord Out;

      BuildSnapV1RecordAdapter(const char *testName,
                               const char *stepName,
                               const char *nodeId,
                               long tick,
                               long scenarioVersion,
                               const char *status)
          : testName_(testName ? testName : ""),
            stepName_(stepName ? stepName : ""),
            nodeId_(nodeId ? nodeId : ""),
            tick_(tick),
            scenarioVersion_(scenarioVersion),
            status_(status ? status : "ok"),
            dirty_(),
            hasTimingFlushMs_(false),
            hasTimingFlushNa_(false),
            hasTimingRecomposeMs_(false),
            hasTimingRecomposeNa_(false),
            hasTimingLayoutMs_(false),
            hasTimingLayoutNa_(false),
            timingFlushMs_(0),
            timingRecomposeMs_(0),
            timingLayoutMs_(0),
            errorCode_(),
            errorMessage_() {}

      BuildSnapV1RecordAdapter &status(const char *value)
      {
        status_ = value ? value : "";
        return *this;
      }

      BuildSnapV1RecordAdapter &dirty(const char *value)
      {
        dirty_ = value ? value : "";
        return *this;
      }

      BuildSnapV1RecordAdapter &timingFlushMs(long value)
      {
        hasTimingFlushMs_ = true;
        hasTimingFlushNa_ = false;
        timingFlushMs_ = value;
        return *this;
      }

      BuildSnapV1RecordAdapter &timingFlushNa()
      {
        hasTimingFlushMs_ = false;
        hasTimingFlushNa_ = true;
        return *this;
      }

      BuildSnapV1RecordAdapter &timingRecomposeMs(long value)
      {
        hasTimingRecomposeMs_ = true;
        hasTimingRecomposeNa_ = false;
        timingRecomposeMs_ = value;
        return *this;
      }

      BuildSnapV1RecordAdapter &timingRecomposeNa()
      {
        hasTimingRecomposeMs_ = false;
        hasTimingRecomposeNa_ = true;
        return *this;
      }

      BuildSnapV1RecordAdapter &timingLayoutMs(long value)
      {
        hasTimingLayoutMs_ = true;
        hasTimingLayoutNa_ = false;
        timingLayoutMs_ = value;
        return *this;
      }

      BuildSnapV1RecordAdapter &timingLayoutNa()
      {
        hasTimingLayoutMs_ = false;
        hasTimingLayoutNa_ = true;
        return *this;
      }

      BuildSnapV1RecordAdapter &errorCode(const char *value)
      {
        errorCode_ = value ? value : "";
        return *this;
      }

      BuildSnapV1RecordAdapter &errorMessage(const char *value)
      {
        errorMessage_ = value ? value : "";
        return *this;
      }

      StepRunStatus run(const int &in, Out &out, FlowError &) const
      {
        (void)in;
        out.setInt("format_version", 1);
        out.setInt("schema_version", 1);
        out.setInt("scenario_version", scenarioVersion_);
        out.set("test", testName_.c_str());
        out.set("step", stepName_.c_str());
        out.set("status", status_.c_str());
        out.setInt("tick", tick_);
        if (!nodeId_.empty())
        {
          out.set("node", nodeId_.c_str());
        }
        if (!dirty_.empty())
        {
          out.set("dirty", dirty_.c_str());
        }
        if (hasTimingFlushMs_)
        {
          out.setInt("timing.flush_ms", timingFlushMs_);
        }
        else if (hasTimingFlushNa_)
        {
          out.set("timing.flush_ms", "na");
        }
        if (hasTimingRecomposeMs_)
        {
          out.setInt("timing.recompose_ms", timingRecomposeMs_);
        }
        else if (hasTimingRecomposeNa_)
        {
          out.set("timing.recompose_ms", "na");
        }
        if (hasTimingLayoutMs_)
        {
          out.setInt("timing.layout_ms", timingLayoutMs_);
        }
        else if (hasTimingLayoutNa_)
        {
          out.set("timing.layout_ms", "na");
        }
        if (!errorCode_.empty())
        {
          out.set("error_code", errorCode_.c_str());
        }
        if (!errorMessage_.empty())
        {
          out.set("error_msg", errorMessage_.c_str());
        }
        return FLOW_STEP_SUCCEEDED;
      }

    private:
      std::string testName_;
      std::string stepName_;
      std::string nodeId_;
      long tick_;
      long scenarioVersion_;
      std::string status_;
      std::string dirty_;
      bool hasTimingFlushMs_;
      bool hasTimingFlushNa_;
      bool hasTimingRecomposeMs_;
      bool hasTimingRecomposeNa_;
      bool hasTimingLayoutMs_;
      bool hasTimingLayoutNa_;
      long timingFlushMs_;
      long timingRecomposeMs_;
      long timingLayoutMs_;
      std::string errorCode_;
      std::string errorMessage_;
    };

    inline StepSpec<BuildSnapV1RecordAdapter> SnapStep(
        int id,
        const char *testName,
        const char *stepName,
        const char *nodeId,
        long tick,
        long scenarioVersion,
        const char *status)
    {
      return Step(id,
                  BuildSnapV1RecordAdapter(testName,
                                           stepName,
                                           nodeId,
                                           tick,
                                           scenarioVersion,
                                           status));
    }

    inline BuildSnapV1RecordAdapter SnapV1(
        const char *testName,
        const char *stepName,
        const char *nodeId,
        long tick,
        long scenarioVersion,
        const char *status)
    {
      return BuildSnapV1RecordAdapter(testName,
                                      stepName,
                                      nodeId,
                                      tick,
                                      scenarioVersion,
                                      status);
    }

    inline BuildSnapV1RecordAdapter SnapV1(
        const char *testName,
        const char *stepName,
        const char *nodeId,
        long tick,
        long scenarioVersion)
    {
      return BuildSnapV1RecordAdapter(testName,
                                      stepName,
                                      nodeId,
                                      tick,
                                      scenarioVersion,
                                      "ok");
    }
  } // namespace dsl
} // namespace loka

#endif // LOKA_DSL_SNAP_FLOW_HPP
