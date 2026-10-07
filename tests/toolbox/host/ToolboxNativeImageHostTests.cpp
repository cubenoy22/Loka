// Host pins for Toolbox native image allocations (#1065): refusals leave no
// gate allocation live, and borrowed pictures are never disposed by Loka.
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

  void verifyPicHandleRefusal(const char *owner, const char *type)
  {
    gKilledPictures = 0;
    loka::core::testing::failLokaAllocRaw(owner, type, 1);
    {
      const loka::core::resource::Image image =
          loka::toolbox::MakeImageFromPicHandle(fakePicture(), 4, 3);
      LOKA_VERIFY(!image.isValid());
    }
    LOKA_VERIFY(gKilledPictures == 0);
    LOKA_VERIFY(loka::core::testing::lokaAllocRawLive() == 0);
    loka::core::testing::allowLokaAllocRaw();
  }

  void testBorrowedPictureKeptOnNativeImageRefusal()
  {
    verifyPicHandleRefusal("ToolboxNativeImage", "Image");
  }

  void testBorrowedPictureKeptOnImageRecordRefusal()
  {
    verifyPicHandleRefusal("Image", "Record");
  }

  void testBorrowedPictureKeptOnControlBlockRefusal()
  {
    verifyPicHandleRefusal("Managed", "ControlBlock");
  }

  void testBorrowedPictureKeptAfterLastImageCopy()
  {
    gKilledPictures = 0;
    loka::core::testing::failLokaAllocRaw("ToolboxNativeImage", "Image", 0);
    {
      loka::core::resource::Image survivingCopy;
      {
        const loka::core::resource::Image image =
            loka::toolbox::MakeImageFromPicHandle(fakePicture(), 4, 3);
        LOKA_VERIFY(image.isValid());
        survivingCopy = image;
      }
      LOKA_VERIFY(survivingCopy.isValid());
      const loka::toolbox::ToolboxNativeImage *native =
          loka::toolbox::TryGetToolboxNativeImage(survivingCopy);
      LOKA_VERIFY(native != 0);
      LOKA_VERIFY(native->kind == loka::toolbox::TOOLBOX_NATIVE_IMAGE_KIND_PICT);
      LOKA_VERIFY(native->payload == fakePicture());
      LOKA_VERIFY(gKilledPictures == 0);
      LOKA_VERIFY(loka::core::testing::lokaAllocRawLive() > 0);
    }
    LOKA_VERIFY(gKilledPictures == 0);
    LOKA_VERIFY(loka::core::testing::lokaAllocRawLive() == 0);
    loka::core::testing::allowLokaAllocRaw();
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
    PIN(testBorrowedPictureKeptOnNativeImageRefusal),
    PIN(testBorrowedPictureKeptOnImageRecordRefusal),
    PIN(testBorrowedPictureKeptOnControlBlockRefusal),
    PIN(testBorrowedPictureKeptAfterLastImageCopy),
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
