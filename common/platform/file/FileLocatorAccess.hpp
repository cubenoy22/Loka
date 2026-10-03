#ifndef LOKA_PLATFORM_FILE_FILELOCATORACCESS_HPP
#define LOKA_PLATFORM_FILE_FILELOCATORACCESS_HPP

#include "core/io/File.hpp"

namespace loka
{
  namespace platform
  {
    namespace file
    {
      /** Platform inclusion seam, not a caller security boundary. No writable
          alias to a published locator is exposed. */
      class FileLocatorAccess
      {
      public:
        /** Copies bytes and builds a complete BASE_NONE File atomically.
            False leaves out untouched. Both identity allocations are nullable;
            display String construction retains its existing allocation contract.
            Zero bytes is a present locator; null is allowed only for zero bytes. */
        static bool capture(const loka::core::String &display, loka::file::File::Kind kind,
                            const void *bytes, std::size_t size, loka::file::File &out)
        {
          typedef loka::file::FileLocator::Payload Payload;
          if ((!bytes && size) || size > static_cast<std::size_t>(-1) - sizeof(Payload))
            return false;
          void *storage = loka::core::LokaAllocRaw(sizeof(Payload) + size, payloadSite());
          if (!storage)
            return false;
          Payload *payload = new (storage) Payload(size);
          if (size)
            std::memcpy(static_cast<void *>(payload + 1), bytes, size);
          loka::core::Managed<Payload> sealed =
              loka::core::Managed<Payload>::TryWrap(payload, &releasePayload, 0);
          if (!sealed.isValid())
          {
            releasePayload(payload, 0);
            return false;
          }
          loka::file::File complete(display);
          complete.kind_ = kind;
          complete.locator_.payload_ = sealed;
          out = complete;
          return true;
        }

        /** Borrows counted const bytes until item is replaced or destroyed.
            False means no locator and leaves both outputs untouched. */
        static bool query(const loka::file::File &item, const unsigned char *&bytes, std::size_t &size)
        {
          if (item.locator_.empty())
            return false;
          bytes = item.locator_.payload_->bytes();
          size = item.locator_.payload_->size;
          return true;
        }

      private:
        static loka::core::LokaAllocationSite payloadSite()
        {
          return loka::core::LokaAllocationSite("FileLocator", "Payload");
        }

        static void releasePayload(loka::file::FileLocator::Payload *payload, void *)
        {
          payload->~Payload();
          loka::core::LokaFreeRaw(payload, payloadSite());
        }
      };
    }
  }
}

#endif
