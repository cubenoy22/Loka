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

      explicit SnapWriteAdapter(const char *path, bool requireV1 = true, const char *defaultNodeId = 0)
          : path_(path ? path : ""), requireV1_(requireV1), defaultNodeId_(defaultNodeId ? defaultNodeId : "") {}

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
        if (!SnapFileWriter::appendRecord(path_.c_str(), out))
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
            hasTimingRecomposeMs_(false),
            hasTimingLayoutMs_(false),
            timingFlushMs_(0),
            timingRecomposeMs_(0),
            timingLayoutMs_(0) {}

      BuildSnapV1RecordAdapter &dirty(const char *value)
      {
        dirty_ = value ? value : "";
        return *this;
      }

      BuildSnapV1RecordAdapter &timingFlushMs(long value)
      {
        hasTimingFlushMs_ = true;
        timingFlushMs_ = value;
        return *this;
      }

      BuildSnapV1RecordAdapter &timingRecomposeMs(long value)
      {
        hasTimingRecomposeMs_ = true;
        timingRecomposeMs_ = value;
        return *this;
      }

      BuildSnapV1RecordAdapter &timingLayoutMs(long value)
      {
        hasTimingLayoutMs_ = true;
        timingLayoutMs_ = value;
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
        if (hasTimingRecomposeMs_)
        {
          out.setInt("timing.recompose_ms", timingRecomposeMs_);
        }
        if (hasTimingLayoutMs_)
        {
          out.setInt("timing.layout_ms", timingLayoutMs_);
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
      bool hasTimingRecomposeMs_;
      bool hasTimingLayoutMs_;
      long timingFlushMs_;
      long timingRecomposeMs_;
      long timingLayoutMs_;
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
