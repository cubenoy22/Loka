#ifndef LOKA_CORE2_RESOURCE_BLOB_HPP
#define LOKA_CORE2_RESOURCE_BLOB_HPP

#include <cstddef>
#include <cstring>
#include "core/LokaAlloc.hpp"
#include "core/Managed.hpp"

namespace loka
{
  namespace core
  {
    namespace resource
    {
      /** Shared record; only Blob can change storage or metadata. */
      class BlobRecord
      {
      public:
        BlobRecord()
            : data_(0),
              length_(0),
              loading_(false),
              completed_(false),
              progress_(-1.0f)
        {
        }

      private:
        friend class Blob;
        BlobRecord(const BlobRecord &);
        BlobRecord &operator=(const BlobRecord &);
        unsigned char *data_;
        std::size_t length_;
        bool loading_;
        bool completed_;
        float progress_;
      };

      /** Shared bytes built through refusing doors, then sealed once for readers. */
      class Blob
      {
      public:
        Blob()
            : handle_()
        {
        }
        static Blob Empty()
        {
          return Blob();
        }
        static Blob Create()
        {
          BlobRecord *record = LokaNew<BlobRecord>(RecordSite());
          if (!record)
            return Blob();
          const Managed<BlobRecord> handle = Managed<BlobRecord>::TryWrap(record, &ReleaseRecord, 0);
          if (!handle.isValid())
          {
            ReleaseRecord(record, 0);
            return Blob();
          }
          return Blob(handle);
        }
        bool isValid() const
        {
          return this->handle_.isValid();
        }
        Managed<BlobRecord> handle() const
        {
          return this->handle_;
        }
        std::size_t size() const
        {
          return this->isValid() ? this->handle_->length_ : 0;
        }
        bool isLoading() const
        {
          return this->isValid() && this->handle_->loading_;
        }
        bool isCompleted() const
        {
          return this->isValid() && this->handle_->completed_;
        }
        float progress() const
        {
          return this->isValid() ? this->handle_->progress_ : UnknownProgress();
        }
        /** Borrows end at a storage-changing success, seal, or last-owner release. */
        const unsigned char *data() const
        {
          return this->size() ? this->handle_->data_ : 0;
        }
        unsigned char *mutableData()
        {
          return this->canWrite() && this->size() ? this->handle_->data_ : 0;
        }
        /** Logical extent; growth copies the prefix before committing. */
        bool tryResize(std::size_t n)
        {
          if (!this->canWrite())
            return false;
          if (n > this->size())
          {
            unsigned char *next = static_cast<unsigned char *>(LokaAllocRaw(n, BytesSite()));
            if (!next)
              return false;
            if (this->size())
              std::memcpy(next, this->handle_->data_, this->size());
            LokaFreeRaw(this->handle_->data_, BytesSite());
            this->handle_->data_ = next;
          }
          else if (!n)
          {
            LokaFreeRaw(this->handle_->data_, BytesSite());
            this->handle_->data_ = 0;
          }
          this->handle_->length_ = n;
          return true;
        }
        /** Copy before release permits self and subrange assignment. */
        bool tryAssign(const unsigned char *p, std::size_t n)
        {
          if (!this->canWrite() || (n && !p))
            return false;
          if (!n)
            return this->tryResize(0);
          unsigned char *next = static_cast<unsigned char *>(LokaAllocRaw(n, BytesSite()));
          if (!next)
            return false;
          std::memcpy(next, p, n);
          LokaFreeRaw(this->handle_->data_, BytesSite());
          this->handle_->data_ = next;
          this->handle_->length_ = n;
          return true;
        }
        void sealBytes()
        {
          if (this->canWrite())
            this->handle_->completed_ = true;
        }
        void setLoading(bool value)
        {
          if (this->canWrite())
            this->handle_->loading_ = value;
        }
        void setProgress(float value)
        {
          if (this->canWrite())
            this->handle_->progress_ = value;
        }
        static float UnknownProgress()
        {
          return -1.0f;
        }
        bool operator==(const Blob &other) const
        {
          return this->handle_ == other.handle_;
        }
        bool operator!=(const Blob &other) const
        {
          return !(*this == other);
        }

      private:
        explicit Blob(const Managed<BlobRecord> &handle)
            : handle_(handle)
        {
        }
        bool canWrite() const
        {
          return this->isValid() && !this->isCompleted();
        }
        static LokaAllocationSite RecordSite()
        {
          return LokaAllocationSite("Blob", "Record");
        }
        static LokaAllocationSite BytesSite()
        {
          return LokaAllocationSite("Blob", "Bytes");
        }
        static void ReleaseRecord(BlobRecord *record, void *)
        {
          LokaFreeRaw(record->data_, BytesSite());
          LokaDelete(record, RecordSite());
        }
        Managed<BlobRecord> handle_;
      };
    } // namespace resource
  } // namespace core
} // namespace loka
#endif
