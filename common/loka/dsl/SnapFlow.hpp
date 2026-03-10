#ifndef LOKA_DSL_SNAP_FLOW_HPP
#define LOKA_DSL_SNAP_FLOW_HPP

#include <cstdio>
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
      FLOW_ERROR_SNAP_WRITE_FAILED = 2,
      FLOW_ERROR_SNAP_LIMIT_EXCEEDED = 3,
      FLOW_ERROR_SNAP_INVALID_OUTPUT_PATH = 4,
      FLOW_ERROR_SNAP_IO_ERROR = 5
    };

    inline const char *snapFlowErrorCodeString(int code)
    {
      switch (code)
      {
      case FLOW_ERROR_SNAP_MISSING_REQUIRED_KEY:
        return "SNAP_MISSING_REQUIRED_KEY";
      case FLOW_ERROR_SNAP_WRITE_FAILED:
        return "SNAP_WRITE_FAILED";
      case FLOW_ERROR_SNAP_LIMIT_EXCEEDED:
        return "SNAP_LIMIT_EXCEEDED";
      case FLOW_ERROR_SNAP_INVALID_OUTPUT_PATH:
        return "SNAP_INVALID_OUTPUT_PATH";
      case FLOW_ERROR_SNAP_IO_ERROR:
        return "SNAP_IO_ERROR";
      default:
        return "SNAP_UNKNOWN_ERROR";
      }
    }

    inline const char *snapFlowErrorMessage(int code)
    {
      switch (code)
      {
      case FLOW_ERROR_SNAP_MISSING_REQUIRED_KEY:
        return "required snap key is missing";
      case FLOW_ERROR_SNAP_WRITE_FAILED:
        return "snap write failed";
      case FLOW_ERROR_SNAP_LIMIT_EXCEEDED:
        return "snap write exceeds configured limits";
      case FLOW_ERROR_SNAP_INVALID_OUTPUT_PATH:
        return "snap output path is invalid";
      case FLOW_ERROR_SNAP_IO_ERROR:
        return "snap I/O operation failed";
      default:
        return "unknown snap error";
      }
    }

    inline std::string snapSourceStepFromId(int stepId)
    {
      char buf[32];
      std::sprintf(buf, "step#%d", stepId);
      return std::string(buf);
    }

    struct SnapFlowErrorSnapshot
    {
      SnapFlowErrorSnapshot()
          : kind(0), code(0), detail(), sourceStep() {}

      void clear()
      {
        kind = 0;
        code = 0;
        detail.clear();
        sourceStep.clear();
      }

      int kind;
      int code;
      std::string detail;
      std::string sourceStep;
    };

    class SnapErrorDetailBuilder
    {
    public:
      SnapErrorDetailBuilder()
          : detail_(), hasPair_(false) {}

      SnapErrorDetailBuilder &add(const char *key, const char *value)
      {
        if (!key || !*key)
        {
          return *this;
        }
        if (hasPair_)
        {
          detail_ += ';';
        }
        detail_ += escapeToken(key);
        detail_ += '=';
        detail_ += escapeToken(value ? value : "");
        hasPair_ = true;
        return *this;
      }

      const std::string &str() const
      {
        return detail_;
      }

    private:
      static std::string escapeToken(const std::string &token)
      {
        std::string escaped;
        escaped.reserve(token.size());
        for (size_t i = 0; i < token.size(); ++i)
        {
          const char c = token[i];
          if (c == '\\' || c == ';' || c == '=')
          {
            escaped += '\\';
          }
          escaped += c;
        }
        return escaped;
      }

      std::string detail_;
      bool hasPair_;
    };

    struct SnapFlowErrorCaptureContext
    {
      SnapFlowErrorCaptureContext()
          : out(0), detail(0), sourceStep(0), sourceStepId(-1) {}

      SnapFlowErrorSnapshot *out;
      const char *detail;
      const char *sourceStep;
      int sourceStepId;
    };

    struct SnapFlowErrorCaptureBuilderContext
    {
      SnapFlowErrorCaptureBuilderContext()
          : out(0), detailBuilder(0), sourceStep(0), sourceStepId(-1) {}

      SnapFlowErrorSnapshot *out;
      const SnapErrorDetailBuilder *detailBuilder;
      const char *sourceStep;
      int sourceStepId;
    };

    inline FlowHandleResult captureSnapFlowError(const FlowError &error, void *user)
    {
      SnapFlowErrorSnapshot *snapshot = static_cast<SnapFlowErrorSnapshot *>(user);
      if (!snapshot)
      {
        return FLOW_ERROR_UNHANDLED;
      }
      snapshot->kind = error.kind;
      snapshot->code = error.code;
      snapshot->detail.clear();
      snapshot->sourceStep.clear();
      return FLOW_ERROR_HANDLED;
    }

    inline FlowHandleResult captureSnapFlowErrorWithDetail(const FlowError &error, void *user)
    {
      SnapFlowErrorCaptureContext *ctx = static_cast<SnapFlowErrorCaptureContext *>(user);
      if (!ctx || !ctx->out)
      {
        return FLOW_ERROR_UNHANDLED;
      }
      ctx->out->kind = error.kind;
      ctx->out->code = error.code;
      ctx->out->detail = ctx->detail ? ctx->detail : "";
      if (ctx->sourceStepId >= 0)
      {
        ctx->out->sourceStep = snapSourceStepFromId(ctx->sourceStepId);
      }
      else
      {
        ctx->out->sourceStep = ctx->sourceStep ? ctx->sourceStep : "";
      }
      return FLOW_ERROR_HANDLED;
    }

    inline FlowHandleResult captureSnapFlowErrorWithDetailBuilder(const FlowError &error, void *user)
    {
      SnapFlowErrorCaptureBuilderContext *ctx = static_cast<SnapFlowErrorCaptureBuilderContext *>(user);
      if (!ctx || !ctx->out)
      {
        return FLOW_ERROR_UNHANDLED;
      }
      ctx->out->kind = error.kind;
      ctx->out->code = error.code;
      if (ctx->detailBuilder)
      {
        ctx->out->detail = ctx->detailBuilder->str();
      }
      else
      {
        ctx->out->detail.clear();
      }
      if (ctx->sourceStepId >= 0)
      {
        ctx->out->sourceStep = snapSourceStepFromId(ctx->sourceStepId);
      }
      else
      {
        ctx->out->sourceStep = ctx->sourceStep ? ctx->sourceStep : "";
      }
      return FLOW_ERROR_HANDLED;
    }

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
        SnapWriteStatus writeStatus = SNAP_WRITE_OK;
        if (hasConfig && (settings.hasMaxTotalBytes || settings.hasMaxFiles))
        {
          writeStatus = SnapFileWriter::appendRecordStatusWithLimits(outputPath.c_str(),
                                                                     out,
                                                                     settings.hasMaxTotalBytes ? settings.maxTotalBytes : 0,
                                                                     settings.hasMaxFiles ? settings.maxFiles : 0);
        }
        else
        {
          writeStatus = SnapFileWriter::appendRecordStatus(outputPath.c_str(), out);
        }
        if (writeStatus != SNAP_WRITE_OK)
        {
          error.kind = FLOW_ERROR_KIND_SNAP;
          error.code = FLOW_ERROR_SNAP_WRITE_FAILED;
          if (writeStatus == SNAP_WRITE_LIMIT_EXCEEDED)
          {
            error.code = FLOW_ERROR_SNAP_LIMIT_EXCEEDED;
          }
          else if (writeStatus == SNAP_WRITE_INVALID_PATH)
          {
            error.code = FLOW_ERROR_SNAP_INVALID_OUTPUT_PATH;
          }
          else if (writeStatus == SNAP_WRITE_IO_ERROR)
          {
            error.code = FLOW_ERROR_SNAP_IO_ERROR;
          }
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
            errorMessage_(),
            errorDetail_(),
            sourceStep_() {}

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

      BuildSnapV1RecordAdapter &snapFlowError(int code)
      {
        this->status("error");
        this->errorCode(snapFlowErrorCodeString(code));
        this->errorMessage(snapFlowErrorMessage(code));
        return *this;
      }

      BuildSnapV1RecordAdapter &snapFlowError(const SnapFlowErrorSnapshot &error)
      {
        this->snapFlowError(error.code);
        if (!error.detail.empty())
        {
          this->errorDetail(error.detail.c_str());
        }
        if (!error.sourceStep.empty())
        {
          this->sourceStep(error.sourceStep.c_str());
        }
        return *this;
      }

      BuildSnapV1RecordAdapter &snapFlowError(int code, const char *detail)
      {
        this->snapFlowError(code);
        this->errorDetail(detail);
        return *this;
      }

      BuildSnapV1RecordAdapter &errorDetail(const char *value)
      {
        errorDetail_ = value ? value : "";
        return *this;
      }

      BuildSnapV1RecordAdapter &sourceStep(const char *value)
      {
        sourceStep_ = value ? value : "";
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
        if (!errorDetail_.empty())
        {
          out.set("error_detail", errorDetail_.c_str());
        }
        if (!sourceStep_.empty())
        {
          out.set("source_step", sourceStep_.c_str());
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
      std::string errorDetail_;
      std::string sourceStep_;
    };

    class BuildSnapErrorV1RecordAdapter
    {
    public:
      typedef SnapFlowErrorSnapshot In;
      typedef SnapRecord Out;

      BuildSnapErrorV1RecordAdapter(const char *testName,
                                    const char *stepName,
                                    const char *nodeId,
                                    long tick,
                                    long scenarioVersion)
          : testName_(testName ? testName : ""),
            stepName_(stepName ? stepName : ""),
            nodeId_(nodeId ? nodeId : ""),
            tick_(tick),
            scenarioVersion_(scenarioVersion) {}

      StepRunStatus run(const In &in, Out &out, FlowError &) const
      {
        BuildSnapV1RecordAdapter base(testName_.c_str(),
                                      stepName_.c_str(),
                                      nodeId_.c_str(),
                                      tick_,
                                      scenarioVersion_,
                                      "error");
        base.snapFlowError(in);

        const int dummy = 0;
        FlowError ignored;
        return base.run(dummy, out, ignored);
      }

    private:
      std::string testName_;
      std::string stepName_;
      std::string nodeId_;
      long tick_;
      long scenarioVersion_;
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

    inline BuildSnapErrorV1RecordAdapter SnapErrorV1(
        const char *testName,
        const char *stepName,
        const char *nodeId,
        long tick,
        long scenarioVersion)
    {
      return BuildSnapErrorV1RecordAdapter(testName,
                                           stepName,
                                           nodeId,
                                           tick,
                                           scenarioVersion);
    }
  } // namespace dsl
} // namespace loka

#endif // LOKA_DSL_SNAP_FLOW_HPP
