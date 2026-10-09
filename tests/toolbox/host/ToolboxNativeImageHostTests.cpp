// Host pins for the Toolbox native image allocations (#1064): every refusal on
// the path from a picture to an Image returns an invalid Image, releases what
// the caller handed over exactly once, and leaves no gate allocation live.
#include "ToolboxNativeImage.hpp"
#include "support/BlobAllocationProbe.hpp"
#include "ToolboxPlatformContext.hpp"
#include "support/LokaAllocFailure.hpp"
#include "support/TestVerify.hpp"
#include <cstdio>
#include <cstring>
#include <vector>

bool ToolboxPlatformContext::openFile(const loka::file::File &,
    loka::platform::file::FileHandle &) const { return false; }

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

  loka::core::resource::Blob pictBlob()
  {
    loka::core::resource::Blob blob = loka::core::resource::Blob::Create();
    std::vector<unsigned char> bytes(21, 0);
    // A 14-byte, 4x3 version-1 picture at absolute offset 7.
    bytes[8] = 14;
    bytes[14] = 3;
    bytes[16] = 4;
    bytes[17] = 0x11;
    bytes[18] = 0x01;
    bytes[20] = 0xff;
    LOKA_VERIFY(blob.tryAssign(bytes.empty() ? 0 : &bytes[0], bytes.size()));
    blob.sealBytes();
    return blob;
  }

  void testImageDecodeRetainsOutputOwnedBlob()
  {
    ToolboxPlatformContext platform;
    loka::core::resource::Image image;
    {
      const loka::core::resource::Blob blob = pictBlob();
      LOKA_VERIFY(platform.createImageFromBlob(blob, 7, 14, image));
    }
    const loka::toolbox::ToolboxNativeImage *native =
        loka::toolbox::TryGetToolboxNativeImage(image);
    LOKA_VERIFY(native != 0);
    const loka::toolbox::ToolboxPictBytesPayload *payload =
        static_cast<const loka::toolbox::ToolboxPictBytesPayload *>(native->payload);
    // Only image owns this Blob. Clearing image destroys the borrowed Blob
    // object as well as its storage unless decode first takes a value copy.
    LOKA_VERIFY(platform.createImageFromBlob(payload->blob, 7, 14, image));
    LOKA_VERIFY(image.isValid() && image.width() == 4 && image.height() == 3);
    native = loka::toolbox::TryGetToolboxNativeImage(image);
    payload = static_cast<const loka::toolbox::ToolboxPictBytesPayload *>(native->payload);
    LOKA_VERIFY(payload->pictureOffset == 7 && payload->pictureEnd == 21);
    LOKA_VERIFY(payload->blob.size() == 21 && payload->blob.data()[20] == 0xff);
  }

  void testPictSnapshotKeepsOnlyFilledRange()
  {
    ToolboxPlatformContext platform;
    loka::core::resource::Blob blob = loka::core::resource::Blob::Create();
    const loka::core::resource::Blob fixture = pictBlob();
    LOKA_VERIFY(blob.tryAssign(fixture.data(), fixture.size()));
    LOKA_VERIFY(blob.size() == 21 && !blob.isCompleted());
    loka::core::resource::Image image;
    LOKA_VERIFY(platform.createImageFromBlob(blob, 7, 14, image));
    blob.mutableData()[20] = 0;
    const loka::toolbox::ToolboxNativeImage *native =
        loka::toolbox::TryGetToolboxNativeImage(image);
    const loka::toolbox::ToolboxPictBytesPayload *payload =
        static_cast<const loka::toolbox::ToolboxPictBytesPayload *>(native->payload);
    LOKA_VERIFY(payload->pictureOffset == 0 && payload->pictureEnd == 14);
    LOKA_VERIFY(payload->blob.size() == 14 && payload->blob.data()[13] == 0xff);
  }

  void testImageDecodeRejectsEmptyAndOutsideRanges()
  {
    ToolboxPlatformContext platform;
    loka::core::resource::Image image;
    const loka::core::resource::Blob invalid;
    const loka::core::resource::Blob empty = loka::core::resource::Blob::Create();
    LOKA_VERIFY(invalid.data() == 0 && invalid.size() == 0);
    LOKA_VERIFY(empty.data() == 0 && empty.size() == 0);
    LOKA_VERIFY(!platform.createImageFromBlob(invalid, 0, 0, image));
    LOKA_VERIFY(!platform.createImageFromBlob(empty, 0, 0, image));
    const loka::core::resource::Blob blob = pictBlob();
    LOKA_VERIFY(!platform.createImageFromBlob(blob, 7, 15, image));
    LOKA_VERIFY(!platform.createImageFromBlob(blob, 22, 1, image));
    LOKA_VERIFY(!platform.createImageFromBlob(blob, 7, 0, image));
    LOKA_VERIFY(!platform.createImageFromBlob(blob, 7, static_cast<std::size_t>(-1), image));
    LOKA_VERIFY(!image.isValid());
  }

  void testPictBytesRefusalsReleaseEverything()
  {
    BlobAllocationProbe allocation;
    const char *const sites[][2] = {
        {"Blob", "Record"}, {"Blob", "Bytes"}, {"Managed", "ControlBlock"},
        {"ToolboxNativeImage", "PictBytes"}, {"ToolboxNativeImage", "Image"}, {"Image", "Record"}
    };
    const loka::core::resource::Blob fixture = pictBlob();
    loka::core::resource::Blob blob = loka::core::resource::Blob::Create();
    LOKA_VERIFY(blob.tryAssign(fixture.data(), fixture.size()));
    const int before = allocation.live;
    for (std::size_t i = 0; i < sizeof(sites) / sizeof(sites[0]); ++i)
    {
      allocation.refuse(sites[i][0], sites[i][1]);
      {
        const loka::core::resource::Image image = loka::toolbox::MakeImageFromPictBlob(blob, 7, 21, 4, 3);
        LOKA_VERIFY(!image.isValid());
      }
      LOKA_VERIFY(allocation.live == before && blob.size() == 21 && blob.data()[20] == 0xff);
    }
    allocation.refuse("Managed", "ControlBlock", 1, 1);
    {
      const loka::core::resource::Image image = loka::toolbox::MakeImageFromPictBlob(blob, 7, 21, 4, 3);
      LOKA_VERIFY(!image.isValid());
    }
    LOKA_VERIFY(allocation.live == before);
    blob.sealBytes();
    allocation.refuse("Blob", "Bytes");
    {
      const loka::core::resource::Image image = loka::toolbox::MakeImageFromPictBlob(blob, 7, 21, 4, 3);
      LOKA_VERIFY(image.isValid() && allocation.remaining == 1);
    }
    LOKA_VERIFY(allocation.live == before);
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
    PIN(testPictBytesRefusalsReleaseEverything),
    PIN(testImageDecodeRetainsOutputOwnedBlob),
    PIN(testPictSnapshotKeepsOnlyFilledRange),
    PIN(testImageDecodeRejectsEmptyAndOutsideRanges)
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
