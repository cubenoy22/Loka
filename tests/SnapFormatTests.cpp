#include "SnapFormatTests.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include "loka/dsl/Flow.hpp"
#include "loka/dsl/SnapFormat.hpp"
#include "loka/dsl/SnapFlow.hpp"

namespace
{
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

  printf("==== [testSnapFlowWriteAdapter] end ====\n");
}
