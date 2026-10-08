#ifndef LOKA_TEST_NATIVE_IMAGE_BLOB_PIN_HPP
#define LOKA_TEST_NATIVE_IMAGE_BLOB_PIN_HPP

#include "app/PlatformContext.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/Image.hpp"
#include "support/TestVerify.hpp"

namespace native_image_blob_pin
{
  using loka::core::resource::Blob;
  using loka::core::resource::Image;

  // Original 2x3 RGB red PNG: IHDR, zlib-compressed filter-0 rows, IEND.
  // Shared by the two native rails so their extent/lifetime cases cannot drift.
  const unsigned char png[] = {
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
      0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03,
      0x08, 0x02, 0x00, 0x00, 0x00, 0x36, 0x88, 0x49, 0xd6, 0x00, 0x00, 0x00,
      0x10, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0x00,
      0x44, 0x0c, 0x28, 0x14, 0x00, 0x44, 0xd0, 0x05, 0xfb, 0xa4, 0xcf, 0xde,
      0x80, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60,
      0x82,
  };
  const std::size_t offset = 7;

  inline void fill(Blob &blob)
  {
    blob.mutableBytes().assign(offset, 0xa5);
    blob.mutableBytes().insert(blob.mutableBytes().end(), png, png + sizeof(png));
    blob.mutableBytes().insert(blob.mutableBytes().end(), 5, 0x5a);
  }

  struct Payload
  {
    Payload() : blob(Blob::Create()) { fill(this->blob); }
    Blob blob;
  private:
    Payload(const Payload &);
    Payload &operator=(const Payload &);
  };

  inline void releasePayload(void *handle, void *userData)
  {
    delete static_cast<Payload *>(handle);
    ++*static_cast<int *>(userData);
  }

  inline void verifyAlias(PlatformContext &platform, Image &image)
  {
    int released = 0;
    Payload *payload = new Payload();
    image = Image::FromNative(payload, 2, 3, &releasePayload, &released);
    LOKA_VERIFY(image.isValid() && released == 0);
    LOKA_VERIFY(payload->blob.isValid() && !payload->blob.isCompleted());
    LOKA_VERIFY(payload->blob.size() == offset + sizeof(png) + 5);
    // handle() temporarily adds one witness; it dies at this semicolon.
    LOKA_VERIFY(payload->blob.handle().useCount() == 2);
    // No Image copy exists. The releaser proves output reset destroyed its
    // sole payload owner during this call; never inspect payload afterwards.
    LOKA_VERIFY(platform.createImageFromBlob(payload->blob, offset, sizeof(png), image));
    LOKA_VERIFY(released == 1);
    LOKA_VERIFY(image.isValid() && image.width() == 2 && image.height() == 3);
  }

  inline void verifyRefusal(PlatformContext &platform, const Blob &blob,
                            std::size_t start, std::size_t length)
  {
    int released = 0;
    Image image = Image::FromNative(new Payload(), 2, 3, &releasePayload, &released);
    LOKA_VERIFY(image.isValid());
    LOKA_VERIFY(!platform.createImageFromBlob(blob, start, length, image));
    LOKA_VERIFY(released == 1);
    LOKA_VERIFY(!image.isValid() && image.nativeHandle() == 0);
    LOKA_VERIFY(image.width() == 0 && image.height() == 0);
  }

  inline void verifyRefusals(PlatformContext &platform)
  {
    const Blob invalid;
    const Blob empty = Blob::Create();
    LOKA_VERIFY(!invalid.isValid() && empty.isValid() && empty.size() == 0);
    verifyRefusal(platform, invalid, 0, 1);
    verifyRefusal(platform, empty, 0, 1);
    Blob filled = Blob::Create();
    fill(filled);
    verifyRefusal(platform, filled, offset, 0);
    verifyRefusal(platform, filled, filled.size() + 1, 1);
    verifyRefusal(platform, filled, offset, filled.size() - offset + 1);
    verifyRefusal(platform, filled, offset, static_cast<std::size_t>(-1));
  }
}

#endif
