#ifndef LOKA_CORE2_RESOURCE_IMAGE_HPP
#define LOKA_CORE2_RESOURCE_IMAGE_HPP

#include <cstddef>
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

        static Image FromNative(
            void *nativeHandle, int width, int height, void (*releaseFn)(void *handle, void *userData), void *userData)
        {
          if (!nativeHandle)
          {
            return Image();
          }
          ImageRecord *record = new ImageRecord();
          record->nativeHandle = nativeHandle;
          record->releaseNative = releaseFn;
          record->releaseUserData = userData;
          record->width = width;
          record->height = height;
          record->format = IMAGE_FORMAT_NATIVE;
          return Image(Managed<ImageRecord>::Wrap(record, &Image::ReleaseRecord));
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
          delete record;
        }

        Managed<ImageRecord> handle_;
      };
    } // namespace resource
  } // namespace core
} // namespace loka

#endif // LOKA_CORE2_RESOURCE_IMAGE_HPP
