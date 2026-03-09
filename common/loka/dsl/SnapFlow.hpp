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
  } // namespace dsl
} // namespace loka

#endif // LOKA_DSL_SNAP_FLOW_HPP
