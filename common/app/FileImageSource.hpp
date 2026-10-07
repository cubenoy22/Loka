#ifndef LOKA_APP_FILE_IMAGE_SOURCE_HPP
#define LOKA_APP_FILE_IMAGE_SOURCE_HPP
#include "app/PlatformReadCapacity.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/Image.hpp"
namespace loka
{
  namespace app
  {
    /** Resolve and read the original File. Refused and located sources never
        retry display text as a path. Output commits only on a successful read. */
    inline loka::platform::file::ReadResult ReadFileImageBlob(PlatformContext *context,
                                                              const loka::file::File &file,
                                                              loka::core::resource::Blob &out)
    {
      using namespace loka::platform::file;
      if (file.base() == loka::file::File::BASE_REFUSED)
        return READ_NO_NATIVE_SPEC;
      FileHandle handle;
      const bool resolved = context && context->openFile(file, handle);
      const bool located = !file.locator().empty();
      if (located && !resolved)
        return READ_NO_NATIVE_SPEC;
      loka::core::resource::Blob blob = loka::core::resource::Blob::Create();
      const PlatformReadCapacity capacity(context);
      ReadResult result;
      if (resolved)
      {
        result = ReadBytes(handle, blob.mutableBytes(), &capacity);
        if (!located && result != READ_OK && result != READ_CAPACITY_REFUSED)
          result = ReadBytes(file.base() == loka::file::File::BASE_APPLICATION
              ? handle.displayPath : file.toString(), blob.mutableBytes(), &capacity);
      }
      else if (file.base() == loka::file::File::BASE_APPLICATION)
        return READ_NO_NATIVE_SPEC; // nothing resolved it, and it has no path to flatten (#1133)
      else
        result = ReadBytes(file.toString(), blob.mutableBytes(), &capacity);
      if (result == READ_OK)
      {
        blob.sealBytes();
        out = blob;
      }
      return result;
    }
    /** Decode this source's entire sealed blob; no pumping or callbacks. */
    inline bool DecodeFileImageBlob(PlatformContext *context,
                                    const loka::core::resource::Blob &blob,
                                    loka::core::resource::Image &image)
    {
      return context && context->createImageFromBlob(blob, 0, blob.bytes().size(), image);
    }
    /** Stable diagnostic vocabulary at the read/decode boundary. */
    inline const char *FileImageReadResultName(loka::platform::file::ReadResult result)
    {
      using namespace loka::platform::file;
      switch (result)
      {
      case READ_OK:
        return "READ_OK";
      case READ_NO_NATIVE_SPEC:
        return "READ_NO_NATIVE_SPEC";
      case READ_NATIVE_OPEN_FAILED:
        return "READ_NATIVE_OPEN_FAILED";
      case READ_NATIVE_SIZE_FAILED:
        return "READ_NATIVE_SIZE_FAILED";
      case READ_NATIVE_READ_FAILED:
        return "READ_NATIVE_READ_FAILED";
      case READ_STDIO_OPEN_FAILED:
        return "READ_STDIO_OPEN_FAILED";
      case READ_STDIO_SEEK_FAILED:
        return "READ_STDIO_SEEK_FAILED";
      case READ_STDIO_READ_FAILED:
        return "READ_STDIO_READ_FAILED";
      case READ_CAPACITY_REFUSED:
        return "READ_CAPACITY_REFUSED";
      case READ_SIZE_OVERFLOW:
        return "READ_SIZE_OVERFLOW";
      }
      return "unknown ReadResult";
    }
  } // namespace app
} // namespace loka
#endif
