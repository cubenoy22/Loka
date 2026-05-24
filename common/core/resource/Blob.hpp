#ifndef LOKA_CORE2_RESOURCE_BLOB_HPP
#define LOKA_CORE2_RESOURCE_BLOB_HPP

#include <cstddef>
#include <vector>

#include "loka/core/Managed.hpp"
#include "loka/core/State.hpp"

namespace loka
{
  namespace core
  {
    namespace resource
    {
      struct BlobRecord
      {
        BlobRecord()
            : data(),
              sizeState(0),
              loadingState(false),
              mutableState(false),
              completedState(false),
              progressState(0.0f)
        {
        }

        std::vector<unsigned char> data;
        MutableState<std::size_t> sizeState;
        MutableState<bool> loadingState;
        MutableState<bool> mutableState;
        MutableState<bool> completedState;
        MutableState<float> progressState;
      };

      class Blob
      {
      public:
        Blob()
            : handle_()
        {
        }
        explicit Blob(const Managed<BlobRecord> &handle)
            : handle_(handle)
        {
        }

        static Blob Empty()
        {
          return Blob(SharedEmptyHandle());
        }

        static Blob Create()
        {
          return Blob(CreateHandle());
        }

        bool isValid() const
        {
          return handle_.isValid();
        }
        Managed<BlobRecord> handle() const
        {
          return handle_;
        }

        std::size_t size() const
        {
          return handle_.isValid() ? handle_->sizeState.get() : 0;
        }

        bool isLoading() const
        {
          return handle_.isValid() ? handle_->loadingState.get() : false;
        }

        bool isMutable() const
        {
          return handle_.isValid() ? handle_->mutableState.get() : false;
        }

        bool isCompleted() const
        {
          return handle_.isValid() ? handle_->completedState.get() : false;
        }

        State<std::size_t> *sizeState() const
        {
          return handle_.isValid() ? &handle_->sizeState : 0;
        }

        State<bool> *loadingState() const
        {
          return handle_.isValid() ? &handle_->loadingState : 0;
        }

        State<bool> *mutableState() const
        {
          return handle_.isValid() ? &handle_->mutableState : 0;
        }

        State<bool> *completedState() const
        {
          return handle_.isValid() ? &handle_->completedState : 0;
        }

        float progress() const
        {
          return handle_.isValid() ? handle_->progressState.get() : UnknownProgress();
        }

        State<float> *progressState() const
        {
          return handle_.isValid() ? &handle_->progressState : 0;
        }

        const std::vector<unsigned char> &bytes() const
        {
          if (handle_.isValid())
          {
            return handle_->data;
          }
          return EmptyBytes();
        }

        std::vector<unsigned char> &mutableBytes()
        {
          ensureHandle();
          return handle_->data;
        }

        void setBytes(const std::vector<unsigned char> &data)
        {
          ensureHandle();
          handle_->data = data;
          handle_->sizeState.set(handle_->data.size());
        }

        void setLoading(bool value)
        {
          ensureHandle();
          handle_->loadingState.set(value);
        }

        void setMutable(bool flag)
        {
          ensureHandle();
          handle_->mutableState.set(flag);
        }

        void setCompleted(bool flag)
        {
          ensureHandle();
          handle_->completedState.set(flag);
        }

        void setProgress(float value)
        {
          ensureHandle();
          handle_->progressState.set(value);
        }

        static float UnknownProgress()
        {
          return -1.0f;
        }

        bool operator==(const Blob &other) const
        {
          return handle_ == other.handle_;
        }
        bool operator!=(const Blob &other) const
        {
          return !(*this == other);
        }

      private:
        static Managed<BlobRecord> CreateHandle()
        {
          BlobRecord *record = new BlobRecord();
          record->sizeState.set(0);
          record->loadingState.set(false);
          record->mutableState.set(false);
          record->completedState.set(false);
          record->progressState.set(UnknownProgress());
          return Managed<BlobRecord>::Wrap(record);
        }

        static Managed<BlobRecord> &SharedEmptyHandle()
        {
          static Managed<BlobRecord> empty = CreateHandle();
          return empty;
        }

        static const std::vector<unsigned char> &EmptyBytes()
        {
          static std::vector<unsigned char> emptyVector;
          return emptyVector;
        }

        void ensureHandle()
        {
          if (!handle_.isValid() || handle_ == SharedEmptyHandle())
          {
            handle_ = CreateHandle();
          }
        }

        Managed<BlobRecord> handle_;
      };
    } // namespace resource
  } // namespace core
} // namespace loka

#endif // LOKA_CORE2_RESOURCE_BLOB_HPP
