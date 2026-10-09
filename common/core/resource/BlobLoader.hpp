#ifndef LOKA_CORE2_RESOURCE_BLOB_LOADER_HPP
#define LOKA_CORE2_RESOURCE_BLOB_LOADER_HPP

#include <cstddef>
#include <cstdio>
#include <vector>

#include "core/State.hpp"
#include "core/resource/Blob.hpp"
#include "core/String.hpp"
#include "platform/file/FileIO.hpp"

namespace loka
{
  namespace core
  {
    namespace resource
    {
      enum BlobSourceType
      {
        BLOB_SOURCE_NONE = 0,
        BLOB_SOURCE_BYTES = 1,
        BLOB_SOURCE_FILE = 2
      };

      struct BlobLoaderRequest
      {
        BlobLoaderRequest()
            : source(BLOB_SOURCE_NONE),
              inlineBytes(),
              filePath(),
              tag(),
              isMutable(false),
              incremental(false)
        {
        }

        BlobLoaderRequest &setFilePath(const String &path)
        {
          source = BLOB_SOURCE_FILE;
          filePath = path;
          return *this;
        }

        BlobLoaderRequest &setFilePath(const char *path)
        {
          return setFilePath(String::Literal(path));
        }

        BlobLoaderRequest &setInlineBytes(const std::vector<unsigned char> &bytes, bool writable)
        {
          source = BLOB_SOURCE_BYTES;
          inlineBytes = bytes;
          isMutable = writable;
          return *this;
        }

        BlobLoaderRequest &setMutableFlag(bool flag)
        {
          isMutable = flag;
          return *this;
        }

        BlobLoaderRequest &setTag(const String &t)
        {
          tag = t;
          return *this;
        }

        BlobLoaderRequest &setTag(const char *t)
        {
          return setTag(String::Literal(t));
        }

        BlobLoaderRequest &setIncremental(bool flag)
        {
          incremental = flag;
          return *this;
        }

        bool operator==(const BlobLoaderRequest &other) const
        {
          if (source != other.source)
            return false;
          if (isMutable != other.isMutable)
            return false;
          if (incremental != other.incremental)
            return false;
          if (!filePath.equals(other.filePath))
            return false;
          if (!tag.equals(other.tag))
            return false;
          return inlineBytes == other.inlineBytes;
        }

        bool operator!=(const BlobLoaderRequest &other) const
        {
          return !(*this == other);
        }

        BlobSourceType source;
        std::vector<unsigned char> inlineBytes;
        String filePath;
        String tag;
        bool isMutable;
        bool incremental;
      };

      class BlobLoader
      {
      public:
        BlobLoader()
            : requestState_(0),
              outputState_(0),
              bound_(false)
        {
        }

        BlobLoader(State<BlobLoaderRequest> *request, MutableState<Blob> *output)
            : requestState_(0),
              outputState_(0),
              bound_(false)
        {
          attach(request, output);
        }

        ~BlobLoader()
        {
          detach();
        }

        void attach(State<BlobLoaderRequest> *request, MutableState<Blob> *output)
        {
          detach();
          requestState_ = request;
          outputState_ = output;
          if (requestState_)
          {
            requestState_->bind(&BlobLoader::RequestChangedThunk, this, true);
            bound_ = true;
          }
        }

        void detach()
        {
          if (requestState_ && bound_)
          {
            requestState_->unbind(&BlobLoader::RequestChangedThunk, this);
          }
          bound_ = false;
          requestState_ = 0;
          outputState_ = 0;
        }

      private:
        static void RequestChangedThunk(void *userData)
        {
          BlobLoader *self = static_cast<BlobLoader *>(userData);
          if (self)
            self->handleRequest();
        }

        void handleRequest()
        {
          if (!outputState_)
            return;
          if (!requestState_)
          {
            outputState_->set(Blob::Empty());
            return;
          }

          BlobLoaderRequest request = requestState_->get();
          if (request.tag.empty() && request.source == BLOB_SOURCE_FILE && !request.filePath.empty())
          {
            request.tag = request.filePath;
          }

          if (request.source == BLOB_SOURCE_NONE)
          {
            outputState_->set(Blob::Empty());
            return;
          }

          Blob blob = Blob::Create();
          if (!blob.isValid())
          {
            outputState_->set(Blob::Empty());
            return;
          }
          blob.setLoading(true);

          if (request.incremental)
            blob.setProgress(Blob::UnknownProgress());
          else
            blob.setProgress(0.0f);

          bool ok = false;
          switch (request.source)
          {
          case BLOB_SOURCE_BYTES:
            ok = blob.tryAssign(request.inlineBytes.empty() ? 0 : &request.inlineBytes[0], request.inlineBytes.size());
            blob.setProgress(1.0f);
            break;
          case BLOB_SOURCE_FILE:
            ok = loka::platform::file::ReadBytes(request.filePath, blob) == loka::platform::file::READ_OK;
            if (ok && !request.incremental)
              blob.setProgress(1.0f);
            break;
          case BLOB_SOURCE_NONE:
            break;
          }

          blob.setLoading(false);
          if (!ok)
          {
            blob.setProgress(Blob::UnknownProgress());
          }
          else if (request.incremental)
          {
            blob.setProgress(Blob::UnknownProgress());
          }

          if (!ok)
          {
            outputState_->set(Blob::Empty());
            return;
          }

          blob.sealBytes();
          outputState_->set(blob);
        }

        State<BlobLoaderRequest> *requestState_;
        MutableState<Blob> *outputState_;
        bool bound_;
      };

    } // namespace resource
  } // namespace core
} // namespace loka

#endif // LOKA_CORE2_RESOURCE_BLOB_LOADER_HPP
