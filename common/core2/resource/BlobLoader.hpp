#ifndef LOKA_CORE2_RESOURCE_BLOB_LOADER_HPP
#define LOKA_CORE2_RESOURCE_BLOB_LOADER_HPP

#include <cstddef>
#include <fstream>
#include <vector>

#include "loka/core/State.hpp"
#include "core2/resource/Blob.hpp"
#include "core2/runtime/ErrorSink.hpp"
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
              errorSink_(0),
              bound_(false),
              localSink_()
        {
          localSink_.addTag(String::Literal("blob-loader"));
        }

        BlobLoader(State<BlobLoaderRequest> *request, MutableState<Blob> *output, runtime::ErrorSink *sink)
            : requestState_(0),
              outputState_(0),
              errorSink_(0),
              bound_(false),
              localSink_()
        {
          localSink_.addTag(String::Literal("blob-loader"));
          attach(request, output, sink);
        }

        ~BlobLoader()
        {
          detach();
        }

        void attach(State<BlobLoaderRequest> *request, MutableState<Blob> *output, runtime::ErrorSink *sink)
        {
          detach();
          requestState_ = request;
          outputState_ = output;
          errorSink_ = sink;
          localSink_.setParent(errorSink_);
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
          errorSink_ = 0;
          localSink_.setParent(0);
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
          localSink_.setTaskLabel(request.tag);

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
            emitError(request, String::Literal("Failed to load blob"));
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
          std::ifstream file(utf8Path.c_str(), std::ios::binary);
          if (!file)
            return false;

          std::vector<unsigned char> bytes;
          file.seekg(0, std::ios::end);
          std::ifstream::pos_type lengthPos = file.tellg();
          if (lengthPos < 0)
          {
            file.clear();
            file.seekg(0, std::ios::beg);
            copyStream(file, &bytes);
          }
          else
          {
            std::size_t length = static_cast<std::size_t>(lengthPos);
            bytes.resize(length);
            file.seekg(0, std::ios::beg);
            if (!bytes.empty())
              file.read(reinterpret_cast<char *>(&bytes[0]), static_cast<std::streamsize>(length));
            if (!file)
            {
              std::streamsize readBytes = file.gcount();
              if (readBytes < 0)
                readBytes = 0;
              bytes.resize(static_cast<std::size_t>(readBytes));
            }
          }

          if (!file && !file.eof())
          {
            return false;
          }

          blob->setBytes(bytes);
          blob->setMutable(writable);
          return true;
        }

        void copyStream(std::ifstream &file, std::vector<unsigned char> *out)
        {
          const std::size_t kBuffer = 4096;
          unsigned char buffer[kBuffer];
          while (file)
          {
            file.read(reinterpret_cast<char *>(buffer), static_cast<std::streamsize>(kBuffer));
            std::streamsize readBytes = file.gcount();
            if (readBytes <= 0)
              break;
            std::size_t oldSize = out->size();
            out->resize(oldSize + static_cast<std::size_t>(readBytes));
            for (std::size_t i = 0; i < static_cast<std::size_t>(readBytes); ++i)
              (*out)[oldSize + i] = buffer[i];
          }
        }

        void emitError(const BlobLoaderRequest &request, const String &message)
        {
          runtime::ErrorSink *sink = errorSink_;
          if (!sink)
            sink = &localSink_;
          runtime::ErrorEvent evt;
          evt.domain = runtime::ERROR_DOMAIN_BLOB;
          evt.code = 1;
          evt.message = message;
          evt.context = request.filePath;
          evt.tags.push_back(String::Literal("blob-loader"));
          if (!request.tag.empty())
            evt.tags.push_back(request.tag);
          sink->push(evt);
        }

        State<BlobLoaderRequest> *requestState_;
        MutableState<Blob> *outputState_;
        runtime::ErrorSink *errorSink_;
        bool bound_;
        runtime::ErrorSink localSink_;
      };

    } // namespace resource
  }   // namespace core
} // namespace loka

#endif // LOKA_CORE2_RESOURCE_BLOB_LOADER_HPP
