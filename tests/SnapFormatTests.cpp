#include "SnapFormatTests.hpp"
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <fstream>
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

  std::ifstream ifs(path, std::ios::binary);
  assert(ifs.good());
  std::string fileContent;
  {
    std::string line;
    while (std::getline(ifs, line))
    {
      fileContent += line;
      fileContent += '\n';
    }
  }
  ifs.close();
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
    std::ifstream ifs(okPath, std::ios::binary);
    assert(ifs.good());
    std::string content;
    std::string line;
    while (std::getline(ifs, line))
    {
      content += line;
      content += '\n';
    }
    assert(content.find("format_version\t1\n") != std::string::npos);
    std::remove(okPath);
  }

  {
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, BuildSnapRecordAdapter(false, true)).input(&input)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(badPath));
    const loka::dsl::FlowRunResult result = chain.runResult();
    assert(result == loka::dsl::FLOW_RUN_FAILED);
    std::ifstream ifs(badPath, std::ios::binary);
    assert(!ifs.good());
    std::remove(badPath);
  }

  {
    const char *autoNodePath = "snap_flow_auto_node.tmp";
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, BuildSnapRecordAdapter(true, false)).input(&input)
        | loka::dsl::Step(2, loka::dsl::SnapWriteAdapter(autoNodePath, true, "FallbackNode"));
    const loka::dsl::FlowRunResult result = chain.runResult();
    assert(result == loka::dsl::FLOW_RUN_SUCCEEDED);
    std::ifstream ifs(autoNodePath, std::ios::binary);
    assert(ifs.good());
    std::string content;
    std::string line;
    while (std::getline(ifs, line))
    {
      content += line;
      content += '\n';
    }
    assert(content.find("node\tFallbackNode\n") != std::string::npos);
    std::remove(autoNodePath);
  }

  {
    const char *cfgPath = "LokaTest.cfg";
    const char *captureDir = "snap_capture_dir";
    const char *captureFile = "snap_cfg_capture.tmp";
    const char *cfgOutputPath = "snap_cfg_adapter.tmp";

    {
      std::ofstream cfg(cfgPath, std::ios::binary);
      cfg << "# Loka test config\n";
      cfg << "capture_dir = " << captureDir << "\n";
      cfg << "max_files = 120\n";
      cfg << "max_total_bytes = 4096\n";
      cfg << "max_files_bad = x\n";
    }

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
    std::ifstream ifs(expectedOutput.c_str(), std::ios::binary);
    assert(ifs.good());
    ifs.close();

    std::remove(expectedOutput.c_str());
    std::remove(cfgPath);
#if defined(_WIN32)
    _rmdir(captureDir);
#else
    rmdir(captureDir);
#endif
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
    std::ifstream ifs(builderPath, std::ios::binary);
    assert(ifs.good());
    std::string content;
    std::string line;
    while (std::getline(ifs, line))
    {
      content += line;
      content += '\n';
    }
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

    std::ifstream ifs(errorPath, std::ios::binary);
    assert(ifs.good());
    std::string content;
    std::string line;
    while (std::getline(ifs, line))
    {
      content += line;
      content += '\n';
    }
    assert(content.find("status\terror\n") != std::string::npos);
    assert(content.find("error_code\tE_TIMEOUT\n") != std::string::npos);
    assert(content.find("error_msg\twait-next-tick timeout\n") != std::string::npos);
    std::remove(errorPath);
  }

  printf("==== [testSnapFlowWriteAdapter] end ====\n");
}
