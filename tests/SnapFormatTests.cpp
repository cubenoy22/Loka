#include "SnapFormatTests.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
#include "loka/dsl/SnapFormat.hpp"

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
