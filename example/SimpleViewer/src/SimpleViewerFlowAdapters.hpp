#ifndef LOKA_SIMPLE_VIEWER_FLOW_ADAPTERS_HPP
#define LOKA_SIMPLE_VIEWER_FLOW_ADAPTERS_HPP

#include <fstream>

#include "app/OpenFileDialog.hpp"
#include "app/PlatformContext.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/BlobLoader.hpp"
#include "core/resource/Image.hpp"
#include "loka/core/String.hpp"
#include "loka/dsl/dsl.hpp"
#include "loka/platform/StringUTF8.hpp"

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
    SIMPLE_VIEWER_FLOW_ERROR_CODE_FILE_READ_FAILED = 1003
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
          message(loka::core::String::Literal("(none)"))
    {
    }

    loka::core::resource::BlobLoaderRequest request;
    loka::core::String message;
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
        }
      }
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }
  };

  struct ProjectionToBlobAdapter
  {
    typedef ChooserProjection In;
    typedef loka::core::resource::Blob Out;

    loka::dsl::StepRunStatus run(const In &projection, Out &out, loka::dsl::FlowError &error) const
    {
      using namespace loka::core::resource;

      if (projection.request.source == BLOB_SOURCE_NONE)
      {
        out = Blob::Empty();
        return loka::dsl::FLOW_STEP_SUCCEEDED;
      }

      if (projection.request.source == BLOB_SOURCE_FILE)
      {
        std::string utf8Path;
        if (!loka::platform::CollectUtf8(projection.request.filePath, utf8Path))
        {
          error.kind = SIMPLE_VIEWER_FLOW_ERROR_BLOB_LOAD;
          error.code = SIMPLE_VIEWER_FLOW_ERROR_CODE_FILE_READ_FAILED;
          return loka::dsl::FLOW_STEP_FAILED;
        }

        std::ifstream file(utf8Path.c_str(), std::ios::binary);
        if (!file)
        {
          error.kind = SIMPLE_VIEWER_FLOW_ERROR_BLOB_LOAD;
          error.code = SIMPLE_VIEWER_FLOW_ERROR_CODE_FILE_READ_FAILED;
          return loka::dsl::FLOW_STEP_FAILED;
        }

        std::vector<unsigned char> bytes;
        file.seekg(0, std::ios::end);
        std::ifstream::pos_type lengthPos = file.tellg();
        if (lengthPos < 0)
        {
          file.clear();
          file.seekg(0, std::ios::beg);
          const std::size_t kBuffer = 4096;
          unsigned char buffer[kBuffer];
          while (file)
          {
            file.read(reinterpret_cast<char *>(buffer), static_cast<std::streamsize>(kBuffer));
            std::streamsize readBytes = file.gcount();
            if (readBytes <= 0)
              break;
            std::size_t oldSize = bytes.size();
            bytes.resize(oldSize + static_cast<std::size_t>(readBytes));
            for (std::size_t i = 0; i < static_cast<std::size_t>(readBytes); ++i)
              bytes[oldSize + i] = buffer[i];
          }
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
          error.kind = SIMPLE_VIEWER_FLOW_ERROR_BLOB_LOAD;
          error.code = SIMPLE_VIEWER_FLOW_ERROR_CODE_FILE_READ_FAILED;
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

    explicit BlobToDecodeAttemptAdapter(PlatformContext *ctx) : ctx_(ctx) {}

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
