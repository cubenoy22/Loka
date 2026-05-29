#ifndef LOKA_SIMPLE_VIEWER_FLOW_ADAPTERS_HPP
#define LOKA_SIMPLE_VIEWER_FLOW_ADAPTERS_HPP

#include <cstdio>

#include "app/OpenFileDialog.hpp"
#include "app/PlatformContext.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/BlobLoader.hpp"
#include "core/resource/Image.hpp"
#include "dsl/flow/Flow.hpp"
#include "loka/core/String.hpp"
#include "loka/platform/StringUTF8.hpp"
#include "platform/file/FileHandle.hpp"
#if defined(LOKA_RETRO68)
#include <Files.h>
#endif

namespace simpleviewer
{
  enum SimpleViewerFlowErrorKind
  {
    SIMPLE_VIEWER_FLOW_ERROR_NONE = 0,
    SIMPLE_VIEWER_FLOW_ERROR_DECODE = 1,
    SIMPLE_VIEWER_FLOW_ERROR_BLOB_LOAD = 2
  };

  enum SimpleViewerFlowErrorCode
  {
    SIMPLE_VIEWER_FLOW_ERROR_CODE_PLATFORM_CONTEXT_MISSING = 1001,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_IMAGE_DECODE_FAILED = 1002,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_FILE_READ_FAILED = 1003,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_NO_FILE_SELECTED = 1004,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_PATH_UTF8_CONVERT_FAILED = 1005,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_PLATFORM_OPENFILE_FAILED = 1006,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_NO_FSSPEC = 1007,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_OPEN_DF_FAILED = 1008,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_GETEOF_FAILED = 1009,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_READ_FAILED = 1010,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_OPEN_FAILED = 1011,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_SEEK_FAILED = 1012,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_READ_FAILED = 1013
  };

  struct ChooserContext
  {
    ChooserContext()
        : result(),
          message(loka::core::String::Literal("(none)"))
    {
    }

    loka::app::FileChooserResult result;
    loka::core::String message;
  };

  struct ChooserProjection
  {
    ChooserProjection()
        : request(),
          message(loka::core::String::Literal("(none)")),
          fileItem(),
          hasFileItem(false)
    {
    }

    loka::core::resource::BlobLoaderRequest request;
    loka::core::String message;
    loka::file::File fileItem;
    bool hasFileItem;
  };

  struct ChooserToContextAdapter
  {
    typedef loka::app::FileChooserResult In;
    typedef ChooserContext Out;

    loka::dsl::StepRunStatus run(const In &result, Out &context, loka::dsl::FlowError &) const
    {
      context = Out();
      context.result = result;
      context.message = formatChooserMessage(result);
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }

  private:
    static loka::core::String formatChooserMessage(const loka::app::FileChooserResult &result)
    {
      using namespace loka::app;
      switch (result.kind)
      {
      case FileChooserResult::RESULT_FILE:
        return loka::core::String::Literal("Loka file: ") + formatItem(result.item);
      case FileChooserResult::RESULT_FOLDER:
        return loka::core::String::Literal("Loka folder: ") + formatItem(result.item);
      case FileChooserResult::RESULT_CANCELED:
        return loka::core::String::Literal("Canceled");
      case FileChooserResult::RESULT_ERROR:
        return loka::core::String::Literal("Error ") + loka::core::String::FromInt(result.errorCode);
      default:
        return loka::core::String::Literal("(none)");
      }
    }

    static loka::core::String formatItem(const loka::file::File &item)
    {
      const loka::core::String path = item.toString();
      return path.empty() ? loka::core::String::Literal("(unknown)") : path;
    }
  };

  struct ContextToProjectionAdapter
  {
    typedef ChooserContext In;
    typedef ChooserProjection Out;

    loka::dsl::StepRunStatus run(const In &context, Out &projection, loka::dsl::FlowError &) const
    {
      projection = Out();
      projection.message = context.message;
      if (context.result.kind == loka::app::FileChooserResult::RESULT_FILE)
      {
        const loka::core::String path = context.result.item.toString();
        if (!path.empty())
        {
          projection.request.setFilePath(path);
          projection.request.setTag(loka::core::String::Literal("image-file"));
          projection.fileItem = context.result.item;
          projection.hasFileItem = true;
        }
      }
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }
  };

  struct ProjectionToBlobAdapter
  {
    typedef ChooserProjection In;
    typedef loka::core::resource::Blob Out;
    explicit ProjectionToBlobAdapter(PlatformContext *ctx)
        : ctx_(ctx)
    {
    }
    ProjectionToBlobAdapter()
        : ctx_(0)
    {
    }

    loka::dsl::StepRunStatus run(const In &projection, Out &out, loka::dsl::FlowError &error) const
    {
      using namespace loka::core::resource;

      if (projection.request.source == BLOB_SOURCE_NONE)
      {
        error.kind = SIMPLE_VIEWER_FLOW_ERROR_BLOB_LOAD;
        error.code = SIMPLE_VIEWER_FLOW_ERROR_CODE_NO_FILE_SELECTED;
        return loka::dsl::FLOW_STEP_FAILED;
      }

      if (projection.request.source == BLOB_SOURCE_FILE)
      {
        std::string utf8Path;
        if (!loka::platform::CollectUtf8(projection.request.filePath, utf8Path))
        {
          error.kind = SIMPLE_VIEWER_FLOW_ERROR_BLOB_LOAD;
          error.code = SIMPLE_VIEWER_FLOW_ERROR_CODE_PATH_UTF8_CONVERT_FAILED;
          return loka::dsl::FLOW_STEP_FAILED;
        }
        std::vector<unsigned char> bytes;
        int detailCode = SIMPLE_VIEWER_FLOW_ERROR_CODE_FILE_READ_FAILED;
        if (!readBytesViaPlatform(projection, bytes, detailCode) && !readFileBytes(utf8Path.c_str(), bytes, detailCode))
        {
          error.kind = SIMPLE_VIEWER_FLOW_ERROR_BLOB_LOAD;
          error.code = detailCode;
          return loka::dsl::FLOW_STEP_FAILED;
        }

        Blob blob = Blob::Create();
        blob.setBytes(bytes);
        blob.setCompleted(true);
        out = blob;
        return loka::dsl::FLOW_STEP_SUCCEEDED;
      }

      // BLOB_SOURCE_BYTES — not used in SimpleViewer but handle gracefully
      out = Blob::Empty();
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }

  private:
    static bool readFileBytes(const char *path, std::vector<unsigned char> &out, int &detailCodeOut)
    {
      out.clear();
      if (!path || !*path)
      {
        detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_OPEN_FAILED;
        return false;
      }
      FILE *file = std::fopen(path, "rb");
      if (!file)
      {
        detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_OPEN_FAILED;
        return false;
      }

      if (std::fseek(file, 0, SEEK_END) == 0)
      {
        long length = std::ftell(file);
        if (length >= 0)
        {
          out.resize(static_cast<std::size_t>(length));
          if (std::fseek(file, 0, SEEK_SET) != 0)
          {
            std::fclose(file);
            out.clear();
            detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_SEEK_FAILED;
            return false;
          }
          if (!out.empty())
          {
            std::size_t readBytes = std::fread(&out[0], 1, out.size(), file);
            if (readBytes != out.size())
            {
              if (std::ferror(file))
              {
                std::fclose(file);
                out.clear();
                detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_READ_FAILED;
                return false;
              }
              out.resize(readBytes);
            }
          }
          std::fclose(file);
          return true;
        }
      }

      if (std::fseek(file, 0, SEEK_SET) != 0)
      {
        std::fclose(file);
        detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_SEEK_FAILED;
        return false;
      }
      const std::size_t kBufferSize = 4096;
      unsigned char buffer[kBufferSize];
      for (;;)
      {
        std::size_t readBytes = std::fread(buffer, 1, kBufferSize, file);
        if (readBytes > 0)
        {
          std::size_t oldSize = out.size();
          out.resize(oldSize + readBytes);
          for (std::size_t i = 0; i < readBytes; ++i)
          {
            out[oldSize + i] = buffer[i];
          }
        }
        if (readBytes < kBufferSize)
        {
          if (std::ferror(file))
          {
            std::fclose(file);
            out.clear();
            detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_READ_FAILED;
            return false;
          }
          break;
        }
      }

      std::fclose(file);
      return true;
    }

    bool readBytesViaPlatform(const In &projection, std::vector<unsigned char> &out, int &detailCodeOut) const
    {
      out.clear();
      if (!ctx_ || !projection.hasFileItem)
      {
        return false;
      }
      loka::platform::file::FileHandle handle;
      if (!ctx_->openFile(projection.fileItem, handle))
      {
        detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_PLATFORM_OPENFILE_FAILED;
        return false;
      }
#if defined(LOKA_RETRO68)
      if (!handle.hasSpec)
      {
        detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_NO_FSSPEC;
        return false;
      }
      if (handle.hasSpec)
      {
        short refNum = 0;
        OSErr err = FSpOpenDF(&handle.spec, fsRdPerm, &refNum);
        if (err != noErr)
        {
          detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_OPEN_DF_FAILED;
          return false;
        }
        long fileSize = 0;
        err = GetEOF(refNum, &fileSize);
        if (err != noErr || fileSize < 0)
        {
          FSClose(refNum);
          detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_GETEOF_FAILED;
          return false;
        }
        out.resize(static_cast<std::size_t>(fileSize));
        if (fileSize > 0)
        {
          long count = fileSize;
          err = FSRead(refNum, &count, &out[0]);
          if (err != noErr && err != eofErr)
          {
            FSClose(refNum);
            out.clear();
            detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_READ_FAILED;
            return false;
          }
          if (count < fileSize)
          {
            out.resize(static_cast<std::size_t>(count < 0 ? 0 : count));
          }
        }
        FSClose(refNum);
        return true;
      }
#endif
      std::string path;
      if (!loka::platform::CollectUtf8(handle.displayPath, path))
      {
        detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_PATH_UTF8_CONVERT_FAILED;
        return false;
      }
      return readFileBytes(path.c_str(), out, detailCodeOut);
    }

    PlatformContext *ctx_;
  };

  struct BlobDecodeAttempt
  {
    BlobDecodeAttempt()
        : image(loka::core::resource::Image::Empty()),
          decoded(false)
    {
    }

    loka::core::resource::Image image;
    bool decoded;
  };

  struct BlobToDecodeAttemptAdapter
  {
    typedef loka::core::resource::Blob In;
    typedef BlobDecodeAttempt Out;

    explicit BlobToDecodeAttemptAdapter(PlatformContext *ctx)
        : ctx_(ctx)
    {
    }

    loka::dsl::StepRunStatus run(const In &blob, Out &attempt, loka::dsl::FlowError &error) const
    {
      attempt = Out();
      if (!this->ctx_)
      {
        error.kind = SIMPLE_VIEWER_FLOW_ERROR_DECODE;
        error.code = SIMPLE_VIEWER_FLOW_ERROR_CODE_PLATFORM_CONTEXT_MISSING;
        return loka::dsl::FLOW_STEP_FAILED;
      }

      if (this->ctx_->createImageFromBlob(blob, attempt.image))
      {
        attempt.decoded = true;
        return loka::dsl::FLOW_STEP_SUCCEEDED;
      }

      error.kind = SIMPLE_VIEWER_FLOW_ERROR_DECODE;
      error.code = SIMPLE_VIEWER_FLOW_ERROR_CODE_IMAGE_DECODE_FAILED;
      return loka::dsl::FLOW_STEP_FAILED;
    }

    PlatformContext *ctx_;
  };

  struct DecodeAttemptToImageAdapter
  {
    typedef BlobDecodeAttempt In;
    typedef loka::core::resource::Image Out;

    loka::dsl::StepRunStatus run(const In &attempt, Out &image, loka::dsl::FlowError &) const
    {
      image = attempt.decoded ? attempt.image : Out::Empty();
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }
  };
} // namespace simpleviewer

#endif // LOKA_SIMPLE_VIEWER_FLOW_ADAPTERS_HPP
