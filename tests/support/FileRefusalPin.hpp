#ifndef LOKA_TESTS_FILE_REFUSAL_PIN_HPP
#define LOKA_TESTS_FILE_REFUSAL_PIN_HPP

#include "support/TestVerify.hpp"
#include "platform/file/FileLocatorAccess.hpp"
#include "platform/file/FileHandle.hpp"

/** Construct a refused value only through the production platform door. */
inline loka::file::File RefusedFileForTest()
{
  typedef loka::file::File File;
  const unsigned char bytes[] = {0, 0xff};
  File located;
  LOKA_VERIFY(loka::platform::file::FileLocatorAccess::capture(
      loka::core::String::Literal("chosen"), File::KIND_FILE, bytes, sizeof(bytes), located));
  return located << File("child");
}

/** Exercise the native rail with an already populated output. */
template <class Context> void VerifyFileRefusal(Context &context)
{
  typedef loka::file::File File;
  const File refused = RefusedFileForTest();
  loka::platform::file::FileHandle out;
  out.displayPath = loka::core::String::Literal("stale");
  out.kind = File::KIND_FOLDER;
#if defined(LOKA_RETRO68)
  out.hasSpec = true;
  std::memset(&out.spec, 0x5a, sizeof(out.spec));
#endif
  LOKA_VERIFY(!context.openFile(refused, out));
  LOKA_VERIFY(out.displayPath.empty() && out.kind == File::KIND_UNKNOWN);
#if defined(LOKA_RETRO68)
  const FSSpec empty = {};
  LOKA_VERIFY(!out.hasSpec && out.spec.vRefNum == empty.vRefNum && out.spec.parID == empty.parID);
  LOKA_VERIFY(std::memcmp(out.spec.name, empty.name, sizeof(empty.name)) == 0);
#endif
}

#endif
