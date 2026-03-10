#include "SnapFormatTests.hpp"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <string>
#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include "loka/dsl/Flow.hpp"
#include "loka/dsl/SnapFormat.hpp"
#include "loka/dsl/SnapFlow.hpp"

namespace
{
  struct SnapFlowErrorCapture
  {
    int kind;
    int code;
  };

  static loka::dsl::FlowHandleResult captureSnapFlowError(const loka::dsl::FlowError &error, void *user)
  {
    SnapFlowErrorCapture *capture = static_cast<SnapFlowErrorCapture *>(user);
    if (capture)
    {
      capture->kind = error.kind;
      capture->code = error.code;
    }
    return loka::dsl::FLOW_ERROR_HANDLED;
  }

  static bool createDirectoryIfMissing(const char *path)
  {
    if (!path || !*path)
    {
      return false;
    }
#if defined(_WIN32)
    const int rc = _mkdir(path);
#else
    const int rc = mkdir(path, 0755);
#endif
    return rc == 0 || errno == EEXIST;
  }

  static bool writeFileBinary(const char *path, const std::string &content)
  {
    FILE *fp = std::fopen(path, "wb");
    if (!fp)
    {
      return false;
    }
    const size_t written = std::fwrite(content.data(), 1, content.size(), fp);
    const int flushResult = std::fflush(fp);
    const int closeResult = std::fclose(fp);
    return written == content.size() && flushResult == 0 && closeResult == 0;
  }

  static bool readFileBinary(const char *path, std::string &out)
  {
    out.clear();
    FILE *fp = std::fopen(path, "rb");
    if (!fp)
    {
      return false;
    }
    char buf[1024];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0)
    {
      out.append(buf, n);
    }
    const bool ok = std::ferror(fp) == 0;
    std::fclose(fp);
    return ok;
  }

  struct BuildSnapRecordAdapter
  {
    typedef int In;
    typedef loka::dsl::SnapRecord Out;

    BuildSnapRecordAdapter(bool valid, bool includeNode)
        : valid_(valid), includeNode_(includeNode) {}

    loka::dsl::StepRunStatus run(const int &in, Out &out, loka::dsl::FlowError &) const
    {
      (void)in;
      out.set("test", "SnapFlow");
      out.set("step", "write");
      if (includeNode_)
      {
        out.set("node", "MainText");
      }
      out.setInt("tick", 1);
      out.set("status", "ok");
      if (valid_)
      {
        out.setInt("format_version", 1);
        out.setInt("schema_version", 1);
        out.setInt("scenario_version", 1);
      }
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }

    bool valid_;
    bool includeNode_;
  };
} // namespace

void testSnapFormatV1()
{
  printf("\n==== [testSnapFormatV1] start ====\n");

  loka::dsl::SnapRecord record;
  record.set("step", "after-wrap");
  record.set("test", "TextWrapRelayout");
  record.set("node", "MainText");
  record.setInt("tick", 12);
  record.set("status", "ok");
  record.setInt("format_version", 1);
  record.setInt("schema_version", 1);
  record.setInt("scenario_version", 3);
  record.set("dirty", "LAYOUT|PROPS");
  record.setInt("timing.flush_ms", 3);
  record.setInt("timing.recompose_ms", 1);
  record.setInt("timing.layout_ms", 2);
  record.set("note", "a\tb\nc\\d");

  std::string missingKey;
  assert(record.validateV1RequiredKeys(missingKey));
  assert(missingKey.empty());

  const std::string serialized = record.serialize(true);
  const std::string expected =
      "dirty\tLAYOUT|PROPS\n"
      "format_version\t1\n"
      "node\tMainText\n"
      "note\ta\\tb\\nc\\\\d\n"
      "scenario_version\t3\n"
      "schema_version\t1\n"
      "status\tok\n"
      "step\tafter-wrap\n"
      "test\tTextWrapRelayout\n"
      "tick\t12\n"
      "timing.flush_ms\t3\n"
      "timing.layout_ms\t2\n"
      "timing.recompose_ms\t1\n"
      "\n";
  assert(serialized == expected);

  const char *path = "snap_format_test.tmp";
  assert(loka::dsl::SnapFileWriter::appendRecord(path, record));

  std::string fileContent;
  assert(readFileBinary(path, fileContent));
  assert(fileContent == expected);
  std::remove(path);

  loka::dsl::SnapRecord incomplete;
  incomplete.set("test", "X");
  assert(!incomplete.validateV1RequiredKeys(missingKey));
  assert(missingKey == "format_version");

  printf("==== [testSnapFormatV1] end ====\n");
}

void testSnapFlowWriteAdapter()
{
  printf("\n==== [testSnapFlowWriteAdapter] start ====\n");
  const char *okPath = "snap_flow_ok.tmp";
  const char *badPath = "snap_flow_bad.tmp";
  const int input = 7;

  {
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, BuildSnapRecordAdapter(true, true)).input(&input)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(okPath));
    const loka::dsl::FlowRunResult result = chain.runResult();
    assert(result == loka::dsl::FLOW_RUN_SUCCEEDED);
    std::string content;
    assert(readFileBinary(okPath, content));
    assert(content.find("format_version\t1\n") != std::string::npos);
    std::remove(okPath);
  }

  {
    SnapFlowErrorCapture capture = {0, 0};
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, BuildSnapRecordAdapter(false, true)).input(&input)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(badPath))
              .onFailure(&captureSnapFlowError, &capture);
    const loka::dsl::FlowRunResult result = chain.runResult();
    assert(result == loka::dsl::FLOW_RUN_SUCCEEDED);
    assert(capture.kind == loka::dsl::FLOW_ERROR_KIND_SNAP);
    assert(capture.code == loka::dsl::FLOW_ERROR_SNAP_MISSING_REQUIRED_KEY);
    std::string content;
    assert(!readFileBinary(badPath, content));
    std::remove(badPath);
  }

  {
    const char *invalidPath = "";
    SnapFlowErrorCapture capture = {0, 0};
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, BuildSnapRecordAdapter(true, true)).input(&input)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(invalidPath))
              .onFailure(&captureSnapFlowError, &capture);
    const loka::dsl::FlowRunResult result = chain.runResult();
    assert(result == loka::dsl::FLOW_RUN_SUCCEEDED);
    assert(capture.kind == loka::dsl::FLOW_ERROR_KIND_SNAP);
    assert(capture.code == loka::dsl::FLOW_ERROR_SNAP_INVALID_OUTPUT_PATH);
  }

  {
    const char *invalidPath = "";
    const char *relayPath = "snap_flow_error_relay.tmp";
    const int inputForRelay = 0;

    loka::dsl::SnapFlowErrorSnapshot snapshot;
    loka::dsl::SnapFlowErrorCaptureContext context;
    context.out = &snapshot;
    context.detail = "while writing relay snap";
    context.sourceStepId = 2;

    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> failingChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, BuildSnapRecordAdapter(true, true)).input(&inputForRelay)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(invalidPath))
              .onFailure(&loka::dsl::captureSnapFlowErrorWithDetail, &context);

    const loka::dsl::FlowRunResult failingResult = failingChain.runResult();
    assert(failingResult == loka::dsl::FLOW_RUN_SUCCEEDED);
    assert(snapshot.kind == loka::dsl::FLOW_ERROR_KIND_SNAP);
    assert(snapshot.code == loka::dsl::FLOW_ERROR_SNAP_INVALID_OUTPUT_PATH);
    assert(snapshot.detail.find("error_kind=1001;error_code=4;extra=") == 0);
    assert(snapshot.detail.find("while writing relay snap") != std::string::npos);

    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> relayChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1,
                          loka::dsl::SnapV1("SnapFlow", "relay", "RelayNode", 99, 2)
                              .snapFlowError(snapshot))
              .input(&inputForRelay)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(relayPath));

    const loka::dsl::FlowRunResult relayResult = relayChain.runResult();
    assert(relayResult == loka::dsl::FLOW_RUN_SUCCEEDED);

    std::string content;
    assert(readFileBinary(relayPath, content));
    assert(content.find("status\terror\n") != std::string::npos);
    assert(content.find("error_code\tSNAP_INVALID_OUTPUT_PATH\n") != std::string::npos);
    assert(content.find("error_msg\tsnap output path is invalid\n") != std::string::npos);
    assert(content.find("error_detail\terror_kind=1001;error_code=4;extra=while writing relay snap\n")
           != std::string::npos);
    assert(content.find("source_step\tstep#2\n") != std::string::npos);
    std::remove(relayPath);

    const char *relayPath2 = "snap_flow_error_relay_adapter.tmp";
    loka::dsl::FlowChain<loka::dsl::SnapFlowErrorSnapshot, loka::dsl::SnapRecord> relayChain2 =
        loka::dsl::Flow()
        | loka::dsl::Step(1,
                          loka::dsl::SnapErrorV1("SnapFlow", "relay-adapter", "RelayNode", 101, 2)
                              .dirty("LAYOUT")
                              .timingFlushMs(5)
                              .timingRecomposeNa()
                              .timingLayoutMs(1))
              .input(&snapshot)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(relayPath2));

    const loka::dsl::FlowRunResult relayResult2 = relayChain2.runResult();
    assert(relayResult2 == loka::dsl::FLOW_RUN_SUCCEEDED);

    std::string content2;
    assert(readFileBinary(relayPath2, content2));
    assert(content2.find("step\trelay-adapter\n") != std::string::npos);
    assert(content2.find("status\terror\n") != std::string::npos);
    assert(content2.find("error_code\tSNAP_INVALID_OUTPUT_PATH\n") != std::string::npos);
    assert(content2.find("source_step\tstep#2\n") != std::string::npos);
    assert(content2.find("dirty\tLAYOUT\n") != std::string::npos);
    assert(content2.find("timing.flush_ms\t5\n") != std::string::npos);
    assert(content2.find("timing.recompose_ms\tna\n") != std::string::npos);
    assert(content2.find("timing.layout_ms\t1\n") != std::string::npos);
    std::remove(relayPath2);
  }

  {
    const char *invalidPath = "";
    const char *relayPath = "snap_flow_error_relay_kv.tmp";
    const int inputForRelay = 0;

    loka::dsl::SnapFlowErrorSnapshot snapshot;
    loka::dsl::SnapErrorDetailBuilder detail;
    detail.add("platform", "Toolbox").add("errno", "28").add("path", "caps=1;tmp");

    loka::dsl::SnapFlowErrorCaptureBuilderContext context;
    context.out = &snapshot;
    context.detailBuilder = &detail;
    context.sourceStepId = 7;

    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> failingChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, BuildSnapRecordAdapter(true, true)).input(&inputForRelay)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(invalidPath))
              .onFailure(&loka::dsl::captureSnapFlowErrorWithDetailBuilder, &context);

    const loka::dsl::FlowRunResult failingResult = failingChain.runResult();
    assert(failingResult == loka::dsl::FLOW_RUN_SUCCEEDED);
    assert(snapshot.code == loka::dsl::FLOW_ERROR_SNAP_INVALID_OUTPUT_PATH);

    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> relayChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1,
                          loka::dsl::SnapV1("SnapFlow", "relay-kv", "RelayNode", 100, 2)
                              .snapFlowError(snapshot))
              .input(&inputForRelay)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(relayPath));

    const loka::dsl::FlowRunResult relayResult = relayChain.runResult();
    assert(relayResult == loka::dsl::FLOW_RUN_SUCCEEDED);

    std::string content;
    assert(readFileBinary(relayPath, content));
    assert(content.find("error_detail\terror_kind=1001;error_code=4;extra=") != std::string::npos);
    assert(content.find("platform\\\\=Toolbox") != std::string::npos);
    assert(content.find("errno\\\\=28") != std::string::npos);
    assert(content.find("path\\\\=caps") != std::string::npos);
    assert(content.find("tmp\n") != std::string::npos);
    assert(content.find("source_step\tstep#7\n") != std::string::npos);
    std::remove(relayPath);
  }

  {
    const char *cfgPath = "LokaTest-io.cfg";
    const char *relativeBadDir = "missing_dir/subdir";
    const char *ioPath = "snap_io_error.tmp";
    assert(writeFileBinary(cfgPath, std::string("capture_dir = ") + relativeBadDir + "\n"));

    SnapFlowErrorCapture capture = {0, 0};
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, BuildSnapRecordAdapter(true, true)).input(&input)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(ioPath, true, 0, cfgPath))
              .onFailure(&captureSnapFlowError, &capture);

    const loka::dsl::FlowRunResult result = chain.runResult();
    assert(result == loka::dsl::FLOW_RUN_SUCCEEDED);
    assert(capture.kind == loka::dsl::FLOW_ERROR_KIND_SNAP);
    assert(capture.code == loka::dsl::FLOW_ERROR_SNAP_IO_ERROR);

    std::remove(cfgPath);
  }

  {
    const char *autoNodePath = "snap_flow_auto_node.tmp";
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, BuildSnapRecordAdapter(true, false)).input(&input)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(autoNodePath, true, "FallbackNode"));
    const loka::dsl::FlowRunResult result = chain.runResult();
    assert(result == loka::dsl::FLOW_RUN_SUCCEEDED);
    std::string content;
    assert(readFileBinary(autoNodePath, content));
    assert(content.find("node\tFallbackNode\n") != std::string::npos);
    std::remove(autoNodePath);
  }

  {
    const char *cfgPath = "LokaTest.cfg";
    const char *captureDir = "snap_capture_dir";
    const char *captureFile = "snap_cfg_capture.tmp";
    const char *cfgOutputPath = "snap_cfg_adapter.tmp";

    assert(writeFileBinary(cfgPath,
                           std::string("# Loka test config\n")
                           + "capture_dir = " + captureDir + "\n"
                           + "max_files = 120\n"
                           + "max_total_bytes = 4096\n"
                           + "max_files_bad = x\n"));

    (void)createDirectoryIfMissing(captureDir);

    loka::dsl::SnapTestConfig::Settings settings;
    assert(loka::dsl::SnapTestConfig::load(cfgPath, settings));
    assert(settings.hasCaptureDir);
    assert(settings.captureDir == std::string(captureDir));
    assert(settings.hasMaxFiles);
    assert(settings.maxFiles == 120);
    assert(settings.hasMaxTotalBytes);
    assert(settings.maxTotalBytes == 4096);

    const std::string resolved = loka::dsl::SnapTestConfig::resolveCapturePath(captureFile, cfgPath);
    assert(resolved == std::string("snap_capture_dir/snap_cfg_capture.tmp"));

    const std::string absoluteUnchanged = loka::dsl::SnapTestConfig::resolveCapturePath("/tmp/loka_abs.snap", cfgPath);
    assert(absoluteUnchanged == std::string("/tmp/loka_abs.snap"));

    const std::string fallback = loka::dsl::SnapTestConfig::resolveCapturePath(captureFile, "missing.cfg");
    assert(fallback == std::string(captureFile));

    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, BuildSnapRecordAdapter(true, true)).input(&input)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(cfgOutputPath, true, 0, cfgPath));

    const loka::dsl::FlowRunResult result = chain.runResult();
    assert(result == loka::dsl::FLOW_RUN_SUCCEEDED);

    const std::string expectedOutput = std::string(captureDir) + "/" + cfgOutputPath;
    std::string content;
    assert(readFileBinary(expectedOutput.c_str(), content));

    std::remove(expectedOutput.c_str());
    std::remove(cfgPath);
#if defined(_WIN32)
    _rmdir(captureDir);
#else
    rmdir(captureDir);
#endif
  }

  {
    const char *cfgPath = "LokaTest-limit.cfg";
    const char *captureDir = "snap_capture_limit_dir";
    const char *limitedPath = "snap_cfg_limit.tmp";

    assert(writeFileBinary(cfgPath,
                           std::string("capture_dir = ") + captureDir + "\n"
                           + "max_total_bytes = 10\n"));

    (void)createDirectoryIfMissing(captureDir);

    SnapFlowErrorCapture capture = {0, 0};
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, BuildSnapRecordAdapter(true, true)).input(&input)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(limitedPath, true, 0, cfgPath))
              .onFailure(&captureSnapFlowError, &capture);

    const loka::dsl::FlowRunResult result = chain.runResult();
    assert(result == loka::dsl::FLOW_RUN_SUCCEEDED);
    assert(capture.kind == loka::dsl::FLOW_ERROR_KIND_SNAP);
    assert(capture.code == loka::dsl::FLOW_ERROR_SNAP_LIMIT_EXCEEDED);

    const std::string expectedOutput = std::string(captureDir) + "/" + limitedPath;
    std::string content;
    assert(!readFileBinary(expectedOutput.c_str(), content));

    std::remove(expectedOutput.c_str());
    std::remove(cfgPath);
#if defined(_WIN32)
    _rmdir(captureDir);
#else
    rmdir(captureDir);
#endif
  }

  {
    const char *cfgPath = "LokaTest-max-files.cfg";
    const char *captureDir = "snap_capture_max_files_dir";
    const char *path = "snap_cfg_max_files.tmp";
    const int inputForMaxFiles = 0;

    assert(writeFileBinary(cfgPath,
                           std::string("capture_dir = ") + captureDir + "\n"
                           + "max_files = 1\n"));

    (void)createDirectoryIfMissing(captureDir);

    {
      loka::dsl::FlowChain<int, loka::dsl::SnapRecord> chain =
          loka::dsl::Flow()
          | loka::dsl::Step(1, loka::dsl::SnapV1("SnapFlow", "first", "NodeA", 1, 2)).input(&inputForMaxFiles)
          | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(path, true, 0, cfgPath));
      assert(chain.runResult() == loka::dsl::FLOW_RUN_SUCCEEDED);
    }

    {
      loka::dsl::FlowChain<int, loka::dsl::SnapRecord> chain =
          loka::dsl::Flow()
          | loka::dsl::Step(1, loka::dsl::SnapV1("SnapFlow", "second", "NodeA", 2, 2)).input(&inputForMaxFiles)
          | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(path, true, 0, cfgPath));
      assert(chain.runResult() == loka::dsl::FLOW_RUN_SUCCEEDED);
    }

    const std::string outputPath = std::string(captureDir) + "/" + path;
    std::string content;
    assert(readFileBinary(outputPath.c_str(), content));
    assert(content.find("step\tsecond\n") != std::string::npos);
    assert(content.find("step\tfirst\n") == std::string::npos);

    std::remove(outputPath.c_str());
    std::remove(cfgPath);
#if defined(_WIN32)
    _rmdir(captureDir);
#else
    rmdir(captureDir);
#endif
  }

  {
    const char *path = "snap_max_bytes_drop_oldest.tmp";

    loka::dsl::SnapRecord r1;
    r1.setInt("format_version", 1);
    r1.setInt("schema_version", 1);
    r1.setInt("scenario_version", 2);
    r1.set("test", "SnapFlow");
    r1.set("step", "one");
    r1.set("node", "NodeA");
    r1.setInt("tick", 1);
    r1.set("status", "ok");

    loka::dsl::SnapRecord r2;
    r2.setInt("format_version", 1);
    r2.setInt("schema_version", 1);
    r2.setInt("scenario_version", 2);
    r2.set("test", "SnapFlow");
    r2.set("step", "two");
    r2.set("node", "NodeA");
    r2.setInt("tick", 2);
    r2.set("status", "ok");

    const long singleBytes = static_cast<long>(r1.serialize(true).size());
    const long maxTotalBytes = singleBytes + 8;

    assert(loka::dsl::SnapFileWriter::appendRecordStatusWithLimits(path, r1, maxTotalBytes, 0)
           == loka::dsl::SNAP_WRITE_OK);
    assert(loka::dsl::SnapFileWriter::appendRecordStatusWithLimits(path, r2, maxTotalBytes, 0)
           == loka::dsl::SNAP_WRITE_OK);

    std::string content;
    assert(readFileBinary(path, content));
    assert(content.find("step\ttwo\n") != std::string::npos);
    assert(content.find("step\tone\n") == std::string::npos);
    std::remove(path);
  }

  {
    const char *path = "snap_combined_limits.tmp";

    loka::dsl::SnapRecord r1;
    r1.setInt("format_version", 1);
    r1.setInt("schema_version", 1);
    r1.setInt("scenario_version", 2);
    r1.set("test", "SnapFlow");
    r1.set("step", "a");
    r1.set("node", "NodeA");
    r1.setInt("tick", 1);
    r1.set("status", "ok");

    loka::dsl::SnapRecord r2;
    r2.setInt("format_version", 1);
    r2.setInt("schema_version", 1);
    r2.setInt("scenario_version", 2);
    r2.set("test", "SnapFlow");
    r2.set("step", "b");
    r2.set("node", "NodeA");
    r2.setInt("tick", 2);
    r2.set("status", "ok");

    loka::dsl::SnapRecord r3;
    r3.setInt("format_version", 1);
    r3.setInt("schema_version", 1);
    r3.setInt("scenario_version", 2);
    r3.set("test", "SnapFlow");
    r3.set("step", "c");
    r3.set("node", "NodeA");
    r3.setInt("tick", 3);
    r3.set("status", "ok");

    const long singleBytes = static_cast<long>(r1.serialize(true).size());
    const long maxTotalBytes = singleBytes * 2 + 8;
    const long maxRecords = 3;

    assert(loka::dsl::SnapFileWriter::appendRecordStatusWithLimits(path, r1, maxTotalBytes, maxRecords)
           == loka::dsl::SNAP_WRITE_OK);
    assert(loka::dsl::SnapFileWriter::appendRecordStatusWithLimits(path, r2, maxTotalBytes, maxRecords)
           == loka::dsl::SNAP_WRITE_OK);
    assert(loka::dsl::SnapFileWriter::appendRecordStatusWithLimits(path, r3, maxTotalBytes, maxRecords)
           == loka::dsl::SNAP_WRITE_OK);

    std::string content;
    assert(readFileBinary(path, content));
    assert(content.find("step\tc\n") != std::string::npos);
    assert(content.find("step\tb\n") != std::string::npos);
    assert(content.find("step\ta\n") == std::string::npos);
    std::remove(path);
  }

  {
    const char *builderPath = "snap_flow_builder.tmp";
    const int inputForBuilder = 0;
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1,
                          loka::dsl::SnapV1("SnapFlow", "builder", "BuilderNode", 9, 2)
                              .dirty("LAYOUT|PROPS")
                              .timingFlushMs(3)
                              .timingRecomposeMs(1)
                              .timingLayoutMs(2))
              .input(&inputForBuilder)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(builderPath));
    const loka::dsl::FlowRunResult result = chain.runResult();
    assert(result == loka::dsl::FLOW_RUN_SUCCEEDED);
    std::string content;
    assert(readFileBinary(builderPath, content));
    assert(content.find("test\tSnapFlow\n") != std::string::npos);
    assert(content.find("step\tbuilder\n") != std::string::npos);
    assert(content.find("node\tBuilderNode\n") != std::string::npos);
    assert(content.find("tick\t9\n") != std::string::npos);
    assert(content.find("scenario_version\t2\n") != std::string::npos);
    assert(content.find("dirty\tLAYOUT|PROPS\n") != std::string::npos);
    assert(content.find("timing.flush_ms\t3\n") != std::string::npos);
    assert(content.find("timing.recompose_ms\t1\n") != std::string::npos);
    assert(content.find("timing.layout_ms\t2\n") != std::string::npos);
    std::remove(builderPath);
  }

  {
    const char *errorPath = "snap_flow_error.tmp";
    const int inputForError = 0;
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1,
                          loka::dsl::SnapV1("SnapFlow", "error", "ErrorNode", 11, 2, "error")
                              .errorCode("E_TIMEOUT")
                              .errorMessage("wait-next-tick timeout"))
              .input(&inputForError)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(errorPath));

    const loka::dsl::FlowRunResult result = chain.runResult();
    assert(result == loka::dsl::FLOW_RUN_SUCCEEDED);

    std::string content;
    assert(readFileBinary(errorPath, content));
    assert(content.find("status\terror\n") != std::string::npos);
    assert(content.find("error_code\tE_TIMEOUT\n") != std::string::npos);
    assert(content.find("error_msg\twait-next-tick timeout\n") != std::string::npos);
    std::remove(errorPath);
  }

  {
    const char *errorAutoPath = "snap_flow_error_auto.tmp";
    const int inputForErrorAuto = 0;
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1,
                          loka::dsl::SnapV1("SnapFlow", "error-auto", "ErrorNode", 13, 2)
                              .snapFlowError(loka::dsl::FLOW_ERROR_SNAP_LIMIT_EXCEEDED, "errno=28"))
              .input(&inputForErrorAuto)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(errorAutoPath));

    const loka::dsl::FlowRunResult result = chain.runResult();
    assert(result == loka::dsl::FLOW_RUN_SUCCEEDED);

    std::string content;
    assert(readFileBinary(errorAutoPath, content));
    assert(content.find("status\terror\n") != std::string::npos);
    assert(content.find("error_code\tSNAP_LIMIT_EXCEEDED\n") != std::string::npos);
    assert(content.find("error_msg\tsnap write exceeds configured limits\n") != std::string::npos);
    assert(content.find("error_detail\terrno=28\n") != std::string::npos);
    std::remove(errorAutoPath);
  }

  {
    const char *errorUnknownPath = "snap_flow_error_unknown.tmp";
    const int inputForUnknown = 0;
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1,
                          loka::dsl::SnapV1("SnapFlow", "error-unknown", "ErrorNode", 14, 2)
                              .snapFlowError(9999))
              .input(&inputForUnknown)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(errorUnknownPath));

    const loka::dsl::FlowRunResult result = chain.runResult();
    assert(result == loka::dsl::FLOW_RUN_SUCCEEDED);

    std::string content;
    assert(readFileBinary(errorUnknownPath, content));
    assert(content.find("error_code\tSNAP_UNKNOWN_ERROR\n") != std::string::npos);
    assert(content.find("error_msg\tunknown snap error\n") != std::string::npos);
    std::remove(errorUnknownPath);
  }

  {
    const char *partialPath = "snap_flow_partial.tmp";
    const int inputForPartial = 0;
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1,
                          loka::dsl::SnapV1("SnapFlow", "partial", "PartialNode", 12, 2)
                              .status("partial")
                              .timingFlushNa()
                              .timingRecomposeNa()
                              .timingLayoutNa())
              .input(&inputForPartial)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(partialPath));

    const loka::dsl::FlowRunResult result = chain.runResult();
    assert(result == loka::dsl::FLOW_RUN_SUCCEEDED);

    std::string content;
    assert(readFileBinary(partialPath, content));
    assert(content.find("status\tpartial\n") != std::string::npos);
    assert(content.find("timing.flush_ms\tna\n") != std::string::npos);
    assert(content.find("timing.recompose_ms\tna\n") != std::string::npos);
    assert(content.find("timing.layout_ms\tna\n") != std::string::npos);
    std::remove(partialPath);
  }

  printf("==== [testSnapFlowWriteAdapter] end ====\n");
}
