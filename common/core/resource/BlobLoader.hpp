#ifndef LOKA_CORE2_RESOURCE_BLOB_LOADER_HPP
#define LOKA_CORE2_RESOURCE_BLOB_LOADER_HPP

#include <cstddef>
#include <cstdio>
#include <vector>

#include "loka/core/State.hpp"
#include "core/resource/Blob.hpp"
#include "loka/core/String.hpp"
#include "loka/platform/StringUTF8.hpp"

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

        bool operator!=(const BlobLoaderRequest &other) const { return !(*this == other); }

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
          blob.setLoading(true);
          blob.setCompleted(false);
          if (request.incremental)
            blob.setProgress(Blob::UnknownProgress());
          else
            blob.setProgress(0.0f);

          bool ok = false;
          switch (request.source)
          {
          case BLOB_SOURCE_BYTES:
            blob.setBytes(request.inlineBytes);
            blob.setMutable(request.isMutable);
            blob.setProgress(1.0f);
            ok = true;
            break;
          case BLOB_SOURCE_FILE:
            ok = loadFile(request.filePath, &blob, request.isMutable);
            if (ok && !request.incremental)
              blob.setProgress(1.0f);
            break;
          default:
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
            blob.setCompleted(false);
            outputState_->set(Blob::Empty());
            return;
          }

          blob.setCompleted(true);
          outputState_->set(blob);
        }

        bool loadFile(const String &path, Blob *blob, bool writable)
        {
          if (!blob)
            return false;
          std::string utf8Path;
          if (!loka::platform::CollectUtf8(path, utf8Path))
            return false;
          FILE *file = std::fopen(utf8Path.c_str(), "rb");
          if (!file)
            return false;

          std::vector<unsigned char> bytes;
          if (std::fseek(file, 0, SEEK_END) == 0)
          {
            long lengthPos = std::ftell(file);
            if (lengthPos >= 0)
            {
              std::size_t length = static_cast<std::size_t>(lengthPos);
              bytes.resize(length);
              if (std::fseek(file, 0, SEEK_SET) != 0)
              {
                std::fclose(file);
                return false;
              }
              if (!bytes.empty())
              {
                const std::size_t readBytes = std::fread(&bytes[0], 1, length, file);
                if (readBytes < length)
                {
                  if (std::ferror(file))
                  {
                    std::fclose(file);
                    return false;
                  }
                  bytes.resize(readBytes);
                }
              }
            }
            else
            {
              if (std::fseek(file, 0, SEEK_SET) != 0)
              {
                std::fclose(file);
                return false;
              }
              if (!copyStream(file, &bytes))
              {
                std::fclose(file);
                return false;
              }
            }
          }
          else
          {
            if (std::fseek(file, 0, SEEK_SET) != 0)
            {
              std::fclose(file);
              return false;
            }
            if (!copyStream(file, &bytes))
            {
              std::fclose(file);
              return false;
            }
          }

          std::fclose(file);

          blob->setBytes(bytes);
          blob->setMutable(writable);
          return true;
        }

        bool copyStream(FILE *file, std::vector<unsigned char> *out)
        {
          const std::size_t kBuffer = 4096;
          unsigned char buffer[kBuffer];
          for (;;)
          {
            const std::size_t readBytes = std::fread(buffer, 1, kBuffer, file);
            if (readBytes == 0)
            {
              if (std::ferror(file))
                return false;
              break;
            }
            const std::size_t oldSize = out->size();
            out->resize(oldSize + readBytes);
            for (std::size_t i = 0; i < readBytes; ++i)
              (*out)[oldSize + i] = buffer[i];
          }
          return true;
        }

        State<BlobLoaderRequest> *requestState_;
        MutableState<Blob> *outputState_;
        bool bound_;
      };

    } // namespace resource
  }   // namespace core
} // namespace loka

#endif // LOKA_CORE2_RESOURCE_BLOB_LOADER_HPP
