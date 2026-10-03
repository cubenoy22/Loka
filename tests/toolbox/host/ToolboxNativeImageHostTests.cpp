// Host pins for the Toolbox native image allocations (#1064): every refusal on
// the path from a picture to an Image returns an invalid Image, releases what
// the caller handed over exactly once, and leaves no gate allocation live.
#include "ToolboxNativeImage.hpp"
#include "support/LokaAllocFailure.hpp"
#include "support/TestVerify.hpp"
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
  int gKilledPictures = 0;
}

void KillPicture(PicHandle)
{
  ++gKilledPictures;
}

namespace
{
  PicHandle fakePicture()
  {
    static Picture picture;
    static PicPtr pointer = &picture;
    return &pointer;
  }

  void verifyPicHandleRefusal(const char *owner, const char *type, bool takeOwnership)
  {
    gKilledPictures = 0;
    loka::core::testing::failLokaAllocRaw(owner, type, 1);
    {
      const loka::core::resource::Image image =
          loka::toolbox::MakeImageFromPicHandle(fakePicture(), 4, 3, takeOwnership);
      LOKA_VERIFY(!image.isValid());
    }
    LOKA_VERIFY(gKilledPictures == (takeOwnership ? 1 : 0));
    LOKA_VERIFY(loka::core::testing::lokaAllocRawLive() == 0);
    loka::core::testing::allowLokaAllocRaw();
  }

  void testOwnedPictureKilledOnceOnNativeImageRefusal()
  {
    verifyPicHandleRefusal("ToolboxNativeImage", "Image", true);
  }

  void testBorrowedPictureKeptOnNativeImageRefusal()
  {
    verifyPicHandleRefusal("ToolboxNativeImage", "Image", false);
  }

  void testOwnedPictureKilledOnceOnImageRecordRefusal()
  {
    verifyPicHandleRefusal("Image", "Record", true);
  }

  void testOwnedPictureKilledOnceOnControlBlockRefusal()
  {
    verifyPicHandleRefusal("Managed", "ControlBlock", true);
  }

  void testPictBytesRefusalsReleaseEverything()
  {
    loka::core::resource::Blob blob = loka::core::resource::Blob::Create();
    blob.setBytes(std::vector<unsigned char>(16, 0));
    blob.setMutable(false);
    blob.setCompleted(true);
    const char *const sites[][2] = {
        {"ToolboxNativeImage", "PictBytes"},
        {"ToolboxNativeImage", "Image"},
        {"Image", "Record"},
        {"Managed", "ControlBlock"},
    };
    for (std::size_t i = 0; i < sizeof(sites) / sizeof(sites[0]); ++i)
    {
      loka::core::testing::failLokaAllocRaw(sites[i][0], sites[i][1], 1);
      {
        const loka::core::resource::Image image = loka::toolbox::MakeImageFromPictBlob(blob, 0, 16, 4, 3);
        LOKA_VERIFY(!image.isValid());
      }
      LOKA_VERIFY(loka::core::testing::lokaAllocRawLive() == 0);
      loka::core::testing::allowLokaAllocRaw();
    }
    loka::core::testing::failLokaAllocRaw("Image", "Record", 0);
    {
      const loka::core::resource::Image image = loka::toolbox::MakeImageFromPictBlob(blob, 0, 16, 4, 3);
      LOKA_VERIFY(image.isValid());
    }
    LOKA_VERIFY(loka::core::testing::lokaAllocRawLive() == 0);
    loka::core::testing::allowLokaAllocRaw();
  }
} // namespace

int main(int argc, char **argv)
{
  struct Test
  {
    const char *name;
    void (*run)();
  };
#define PIN(name) {#name, &name}
  const Test tests[] = {
    PIN(testOwnedPictureKilledOnceOnNativeImageRefusal),
    PIN(testBorrowedPictureKeptOnNativeImageRefusal),
    PIN(testOwnedPictureKilledOnceOnImageRecordRefusal),
    PIN(testOwnedPictureKilledOnceOnControlBlockRefusal),
    PIN(testPictBytesRefusalsReleaseEverything)
  };
#undef PIN
  bool ran = false;
  for (std::size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i)
    if (argc == 1 || std::strcmp(argv[1], tests[i].name) == 0)
    {
      tests[i].run();
      std::printf("PASS %s\n", tests[i].name);
      ran = true;
    }
  return ran ? 0 : 1;
}
