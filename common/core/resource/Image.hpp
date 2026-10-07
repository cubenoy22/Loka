#ifndef LOKA_CORE2_RESOURCE_IMAGE_HPP
#define LOKA_CORE2_RESOURCE_IMAGE_HPP

#include <cstddef>
#include "core/LokaAlloc.hpp"
#include "core/Managed.hpp"

namespace loka
{
  namespace core
  {
    namespace resource
    {
      enum ImageFormat
      {
        IMAGE_FORMAT_UNKNOWN = 0,
        IMAGE_FORMAT_NATIVE = 1
      };

      struct ImageRecord
      {
        ImageRecord()
            : nativeHandle(0),
              releaseNative(0),
              releaseUserData(0),
              width(0),
              height(0),
              format(IMAGE_FORMAT_UNKNOWN)
        {
        }

        void *nativeHandle;
        void (*releaseNative)(void *handle, void *userData);
        void *releaseUserData;
        int width;
        int height;
        ImageFormat format;
      };

      class Image
      {
      public:
        Image()
            : handle_()
        {
        }
        explicit Image(const Managed<ImageRecord> &handle)
            : handle_(handle)
        {
        }

        static Image Empty()
        {
          return Image();
        }

        /** Consumes nativeHandle. When the record or its control block cannot be
            allocated, releaseFn runs once on the handle and the Image is invalid;
            this never aborts (#1064). */
        static Image FromNative(
            void *nativeHandle, int width, int height, void (*releaseFn)(void *handle, void *userData), void *userData)
        {
          if (!nativeHandle)
          {
            return Image();
          }
          ImageRecord *record = LokaNew<ImageRecord>(RecordSite());
          if (!record)
          {
            if (releaseFn)
            {
              releaseFn(nativeHandle, userData);
            }
            return Image();
          }
          record->nativeHandle = nativeHandle;
          record->releaseNative = releaseFn;
          record->releaseUserData = userData;
          record->width = width;
          record->height = height;
          record->format = IMAGE_FORMAT_NATIVE;
          const Managed<ImageRecord> handle = Managed<ImageRecord>::TryWrap(record, &Image::ReleaseRecord, 0);
          if (!handle.isValid())
          {
            // TryWrap left the record with us: release it as the last owner would.
            ReleaseRecord(record, 0);
            return Image();
          }
          return Image(handle);
        }

        bool isValid() const
        {
          return handle_.isValid() && handle_->nativeHandle;
        }

        void *nativeHandle() const
        {
          return handle_.isValid() ? handle_->nativeHandle : 0;
        }

        int width() const
        {
          return handle_.isValid() ? handle_->width : 0;
        }

        int height() const
        {
          return handle_.isValid() ? handle_->height : 0;
        }

        ImageFormat format() const
        {
          return handle_.isValid() ? handle_->format : IMAGE_FORMAT_UNKNOWN;
        }

        bool operator==(const Image &other) const
        {
          return handle_ == other.handle_;
        }
        bool operator!=(const Image &other) const
        {
          return !(*this == other);
        }

      private:
        static void ReleaseRecord(ImageRecord *record, void *)
        {
          if (!record)
          {
            return;
          }
          if (record->releaseNative && record->nativeHandle)
          {
            record->releaseNative(record->nativeHandle, record->releaseUserData);
          }
          LokaDelete(record, RecordSite());
        }

        static LokaAllocationSite RecordSite()
        {
          return LokaAllocationSite("Image", "Record");
        }

        Managed<ImageRecord> handle_;
      };
    } // namespace resource
  } // namespace core
} // namespace loka

#endif // LOKA_CORE2_RESOURCE_IMAGE_HPP
