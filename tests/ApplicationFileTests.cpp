#include "ApplicationFileTests.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "core/io/File.hpp"
#include "platform/file/AppLocation.hpp"
#include "platform/file/FileHandle.hpp"
#include "platform/file/FileIO.hpp"
#include "platform/null/NullPlatformContext.hpp"

namespace
{
  bool CreateDirectoryForTest(const char *path)
  {
#if defined(_WIN32)
    return _mkdir(path) == 0;
#else
    return mkdir(path, 0755) == 0;
#endif
  }

  bool RemoveDirectoryForTest(const char *path)
  {
#if defined(_WIN32)
    return _rmdir(path) == 0;
#else
    return rmdir(path) == 0;
#endif
  }

  void WriteBytes(const char *path, const unsigned char *bytes, std::size_t size)
  {
    std::FILE *file = std::fopen(path, "wb");
    assert(file);
    assert(std::fwrite(bytes, 1, size, file) == size);
    assert(std::fclose(file) == 0);
  }
} // namespace

void testApplicationFileCompositionKeepsExplicitBase()
{
  loka::file::File asset = loka::file::File::Application() << loka::file::File("ASSETS.LRP");
  assert(asset.base() == loka::file::File::BASE_APPLICATION);
  assert(asset.relativePath().equals(loka::core::String::Literal("ASSETS.LRP")));

  loka::file::File desktopChild = loka::file::File::Desktop() << loka::file::File("manual.pdf");
  loka::file::File childWins = loka::file::File::Application() << desktopChild;
  assert(childWins.base() == loka::file::File::BASE_DESKTOP);
  assert(childWins.relativePath().equals(loka::core::String::Literal("manual.pdf")));
}

void testApplicationRelativePredicateEnforcesSingleSegment()
{
  using loka::platform::file::ApplicationRelativeIsOpenable;

  assert(ApplicationRelativeIsOpenable(loka::file::File::Application() << loka::file::File("ASSETS.LRP")));
  assert(!ApplicationRelativeIsOpenable(loka::file::File::Application()));
  assert(!ApplicationRelativeIsOpenable(loka::file::File::Application() << loka::file::File("a/b")));
  assert(!ApplicationRelativeIsOpenable(loka::file::File::Application() << loka::file::File("a:b")));
  assert(!ApplicationRelativeIsOpenable(loka::file::File::Application() << loka::file::File("sub\\a.bin")));
  assert(!ApplicationRelativeIsOpenable(loka::file::File::Application() << loka::file::File("..\\..\\secret")));
  assert(!ApplicationRelativeIsOpenable(
      loka::file::File::Application()
      << loka::file::File(loka::core::String::Utf8("a\0b", 3))));
  assert(!ApplicationRelativeIsOpenable(loka::file::File::Application() << loka::file::File(".")));
  assert(!ApplicationRelativeIsOpenable(loka::file::File::Application() << loka::file::File("..")));
  assert(!ApplicationRelativeIsOpenable(loka::file::File("ASSETS.LRP")));

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(LOKA_RETRO68)
  loka::platform::file::FileHandle handle;
  assert(!loka::platform::file::ResolveApplicationItem(
      loka::file::File::Application() << loka::file::File("ASSETS.LRP"), handle));
#endif
}

void testNullApplicationFileUsesOnlyInjectedDirectory()
{
  const char *directory = "_loka_application_fixture";
  const char *name = "_loka_application_asset.bin";
  const char *injectedPath = "_loka_application_fixture/_loka_application_asset.bin";
  const unsigned char cwdBytes[] = {0x43, 0x57, 0x44};
  const unsigned char injectedBytes[] = {0x41, 0x50, 0x50};

  std::remove(injectedPath);
  RemoveDirectoryForTest(directory);
  std::remove(name);
  assert(CreateDirectoryForTest(directory));
  WriteBytes(name, cwdBytes, sizeof(cwdBytes));
  WriteBytes(injectedPath, injectedBytes, sizeof(injectedBytes));

  const loka::file::File item = loka::file::File::Application() << loka::file::File(name);
  NullPlatformContext noInjection;
  loka::platform::file::FileHandle handle;
  assert(!noInjection.openFile(item, handle));

  NullPlatformContext context;
  context.setApplicationDirectory(loka::core::String::Literal(directory));
  assert(!context.openFile(loka::file::File::Application(), handle));
  assert(!context.openFile(loka::file::File::Application() << loka::file::File("a/b"), handle));
  assert(!context.openFile(loka::file::File::Application() << loka::file::File(".."), handle));
  assert(context.openFile(item, handle));
  assert(handle.displayPath.equals(loka::core::String::Literal(injectedPath)));

  std::FILE *opened = loka::platform::file::OpenRead(handle.displayPath);
  assert(opened);
  unsigned char actual[sizeof(injectedBytes)] = {0};
  assert(std::fread(actual, 1, sizeof(actual), opened) == sizeof(actual));
  assert(std::fclose(opened) == 0);
  assert(std::memcmp(actual, injectedBytes, sizeof(actual)) == 0);

  assert(std::remove(injectedPath) == 0);
  assert(RemoveDirectoryForTest(directory));
  assert(std::remove(name) == 0);
}
