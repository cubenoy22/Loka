#include "ToolboxFileHost.hpp"
#include "ToolboxPlatformContext.hpp"
#include "ToolboxByteSource.hpp"
#include "app/FileImageSource.hpp"
#include "support/TestVerify.hpp"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <unistd.h>

// Only unrelated UI/heap virtuals are substituted. openFile and registration
// are linked from the same production translation unit as Classic builds.
ToolboxPlatformContext::ToolboxPlatformContext() {}
ToolboxPlatformContext::~ToolboxPlatformContext() {}
App *ToolboxPlatformContext::createApp(AppConfigurable *, NativeModuleHandle, int) const { return 0; }
Window *ToolboxPlatformContext::createWindow(const WindowProps &) { return 0; }
loka::app::scene::NodeContext *ToolboxPlatformContext::createNodeContext(loka::app::scene::Node *) const { return 0; }
bool ToolboxPlatformContext::queryLargestContiguousAllocation(std::size_t &) const { return false; }
bool ToolboxPlatformContext::createImageFromBlob(const loka::core::resource::Blob &, std::size_t,
    std::size_t, loka::core::resource::Image &) const { return false; }

using loka::file::File;
using loka::core::String;
using namespace loka::platform::file;
using namespace toolbox_file_host;

typedef char ClassicSpecSize[sizeof(FSSpec) == 70 ? 1 : -1];
typedef char ClassicParentOffset[offsetof(FSSpec, parID) == 2 ? 1 : -1];
typedef char ClassicNameOffset[offsetof(FSSpec, name) == 6 ? 1 : -1];

// Dialog-equivalent input at its existing production registration door.
// UI presentation and State delivery are not simulated by this fixture.
static File Choose(const FSSpec &spec)
{
  const String display = String::Utf8(reinterpret_cast<const char *>(spec.name + 1), spec.name[0]);
  File chosen(display);
  chosen.setKind(File::KIND_FILE);
  ToolboxPlatformContext::registerChosenFileSpec(display, spec);
  return chosen;
}
static std::string Read(ToolboxPlatformContext &context, const File &file)
{
  FileHandle handle;
  LOKA_VERIFY(context.openFile(file, handle));
  LOKA_VERIFY(handle.hasSpec);
  std::vector<unsigned char> bytes;
  LOKA_VERIFY(ReadBytes(handle, bytes) == READ_OK);
  LOKA_VERIFY(OpenCount() == 0);
  return std::string(bytes.begin(), bytes.end());
}
static void Tuple()
{
  const FSSpec specs[] = { Spec(-7, 0x12345678, "Photo.PICT"),
    Spec(-7, 0x12345679, "Photo.PICT"), Spec(-8, 0x12345678, "Photo.PICT"),
    Spec(-7, 0x12345678, std::string("Photo\0PICT", 10)) };
  for (unsigned i = 0; i < 4; ++i) Put(specs[i], std::string(3, static_cast<char>('A' + i)));
  for (unsigned i = 0; i < 4; ++i)
  {
    FSSpec poisoned = specs[i];
    std::memset(poisoned.name + poisoned.name[0] + 1, 0xAD, 63 - poisoned.name[0]);
    loka::toolbox::ToolboxByteSource source;
    LOKA_VERIFY(source.open(poisoned));
    std::size_t size = 0;
    LOKA_VERIFY(source.size(size));
    LOKA_VERIFY(size == 3);
    unsigned char byte = 0;
    LOKA_VERIFY(source.readAt(0, &byte, 1));
    LOKA_VERIFY(byte == 'A' + i);
    LOKA_VERIFY(source.readAt(2, &byte, 1));
    LOKA_VERIFY(!source.readAt(3, &byte, 1));
    source.close();
    LOKA_VERIFY(OpenCount() == 0);
  }
  {
    loka::toolbox::ToolboxByteSource first;
    loka::toolbox::ToolboxByteSource second;
    LOKA_VERIFY(first.open(specs[0]));
    LOKA_VERIFY(second.open(specs[1]));
    LOKA_VERIFY(OpenCount() == 2);
    unsigned char bytes[2] = {};
    LOKA_VERIFY(first.readAt(1, bytes, 2));
    LOKA_VERIFY(bytes[0] == 'A' && bytes[1] == 'A');
    LOKA_VERIFY(second.readAt(1, bytes, 2));
    LOKA_VERIFY(bytes[0] == 'B' && bytes[1] == 'B');
  }
  LOKA_VERIFY(OpenCount() == 0); // Destruction closes both independent marks.
  short ref = 0;
  LOKA_VERIFY(FSpOpenDF(&specs[0], fsRdPerm, &ref) == noErr);
  unsigned char partial[4] = {};
  long count = 4;
  LOKA_VERIFY(FSRead(ref, &count, partial) == eofErr);
  LOKA_VERIFY(count == 3 && partial[0] == 'A' && partial[2] == 'A');
  count = 1;
  LOKA_VERIFY(FSRead(ref, &count, partial) == eofErr);
  LOKA_VERIFY(count == 0);
  LOKA_VERIFY(FSClose(ref) == noErr);
  LOKA_VERIFY(FSClose(ref) == paramErr);
  loka::toolbox::ToolboxByteSource source;
  LOKA_VERIFY(!source.open(Spec(-7, 999, "Photo.PICT")));
}
static void Single()
{
  ToolboxPlatformContext context;
  const FSSpec spec = Spec(-7, 0x12345678, "Single.PICT");
  Put(spec, "chosen contents");
  const File chosen = Choose(spec);
  LOKA_VERIFY(Read(context, chosen) == "chosen contents");
  FileHandle handle;
  LOKA_VERIFY(context.openFile(chosen, handle));
  LOKA_VERIFY(handle.spec.vRefNum == spec.vRefNum && handle.spec.parID == spec.parID);
  LOKA_VERIFY(handle.kind == File::KIND_FILE);
}
static void Application(const bool viewer, const bool smirky)
{
  ToolboxPlatformContext context;
  SetApplication(Spec(-2, 0x23456789, "App"));
  const File chosen("Fixture.PICT");
  Put(Spec(-2, 0x23456789, "Fixture.PICT"), "application contents");
  const File item = File::Application() << chosen;
  LOKA_VERIFY(Read(context, item) == "application contents");
  FileHandle handle;
  LOKA_VERIFY(context.openFile(item, handle));
  LOKA_VERIFY(handle.spec.vRefNum == -2 && handle.spec.parID == 0x23456789);
  if (viewer)
  {
    // SimpleViewerScenarioDriver::openImage probes the data fork first.
    short ref = 0;
    LOKA_VERIFY(FSpOpenDF(&handle.spec, fsRdPerm, &ref) == noErr);
    long size = 0;
    LOKA_VERIFY(GetEOF(ref, &size) == noErr);
    LOKA_VERIFY(FSClose(ref) == noErr);
    LOKA_VERIFY(size == 20);
  }
  if (viewer || smirky)
  {
    // Same resolve/register sequence as both scenario stand-ins. Their
    // controller/JS delivery is covered elsewhere, not compiled here.
    ToolboxPlatformContext::registerChosenFileSpec(chosen.toString(), handle.spec);
    LOKA_VERIFY(Read(context, chosen) == "application contents");
  }
  LOKA_VERIFY(!context.openFile(File::Application(), handle));
  LOKA_VERIFY(!handle.hasSpec);
}
static void Collision(bool reverse)
{
  ToolboxPlatformContext context;
  const FSSpec a = Spec(-7, 101, "Photo.PICT");
  const FSSpec b = Spec(-7, 202, "Photo.PICT");
  Put(a, "A contents");
  Put(b, "B contents");
  const File retained = Choose(reverse ? b : a);
  Choose(reverse ? a : b);
  const std::string actual = Read(context, retained);
  const std::string expected = reverse ? "B contents" : "A contents";
  std::fprintf(stderr, "retained choice: expected '%s', read '%s'\n", expected.c_str(), actual.c_str());
  LOKA_VERIFY(actual == expected);
}
static void Decoy()
{
  // Private temporary directory prevents collisions or overwriting user files.
  char directory[] = "/tmp/loka-file-choice-XXXXXX";
  LOKA_VERIFY(mkdtemp(directory) != 0);
  const std::string path = std::string(directory) + "/Photo.PICT";
  std::FILE *stream = std::fopen(path.c_str(), "wb");
  LOKA_VERIFY(stream != 0);
  LOKA_VERIFY(std::fwrite("decoy", 1, 5, stream) == 5);
  LOKA_VERIFY(std::fclose(stream) == 0);
  char cwd[4096];
  LOKA_VERIFY(getcwd(cwd, sizeof(cwd)) != 0);
  LOKA_VERIFY(chdir(directory) == 0);
  ToolboxPlatformContext context;
  const File chosen = Choose(Spec(-7, 999, "Photo.PICT")); // No native file installed.
  FileHandle handle;
  LOKA_VERIFY(context.openFile(chosen, handle));
  std::vector<unsigned char> bytes;
  LOKA_VERIFY(ReadBytes(handle, bytes) == READ_NATIVE_OPEN_FAILED);
  loka::core::resource::Blob blob;
  const ReadResult result = loka::app::ReadFileImageBlob(&context, &handle, chosen.toString(), blob);
  const std::string actual(blob.bytes().begin(), blob.bytes().end());
  LOKA_VERIFY(chdir(cwd) == 0);
  LOKA_VERIFY(std::remove(path.c_str()) == 0);
  LOKA_VERIFY(rmdir(directory) == 0);
  std::fprintf(stderr, "native open failed; fallback result=%s contents='%s'\n",
      loka::app::FileImageReadResultName(result), actual.c_str());
  LOKA_VERIFY(result == READ_NATIVE_OPEN_FAILED);
  LOKA_VERIFY(actual != "decoy");
}
int main(int argc, char **argv)
{
  LOKA_VERIFY(argc == 2);
  if (!std::strcmp(argv[1], "tuple")) Tuple();
  else if (!std::strcmp(argv[1], "single")) Single();
  else if (!std::strcmp(argv[1], "application")) Application(false, false);
  else if (!std::strcmp(argv[1], "viewer")) Application(true, false);
  else if (!std::strcmp(argv[1], "smirky")) Application(false, true);
  else if (!std::strcmp(argv[1], "collision-ab")) Collision(false);
  else if (!std::strcmp(argv[1], "collision-ba")) Collision(true);
  else if (!std::strcmp(argv[1], "decoy")) Decoy();
  else return 2;
  LOKA_VERIFY(OpenCount() == 0);
  return 0;
}
