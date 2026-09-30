#ifndef LOKA_SIMPLE_VIEWER_FLOW_ADAPTERS_HPP
#define LOKA_SIMPLE_VIEWER_FLOW_ADAPTERS_HPP

#include <cstdio>

#include "app/OpenFileDialog.hpp"
#include "app/PlatformContext.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/BlobLoader.hpp"
#include "core/resource/Image.hpp"
#include "dsl/flow/Flow.hpp"
#include "core/String.hpp"
#include "platform/file/FileHandle.hpp"
#include "platform/file/FileIO.hpp"

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
    SIMPLE_VIEWER_FLOW_ERROR_CODE_PLATFORM_OPENFILE_FAILED = 1006,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_NO_FSSPEC = 1007,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_OPEN_DF_FAILED = 1008,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_GETEOF_FAILED = 1009,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_READ_FAILED = 1010,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_OPEN_FAILED = 1011,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_SEEK_FAILED = 1012,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_READ_FAILED = 1013,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_IMAGE_LOAD_REQUIRES_RELEASE = 1014
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

  struct ProjectionToBlobAdapter : private loka::platform::file::ReadCapacity
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
      out = Out::Empty();

      if (projection.request.source == BLOB_SOURCE_NONE)
      {
        error.kind = SIMPLE_VIEWER_FLOW_ERROR_BLOB_LOAD;
        error.code = SIMPLE_VIEWER_FLOW_ERROR_CODE_NO_FILE_SELECTED;
        return loka::dsl::FLOW_STEP_FAILED;
      }

      if (projection.request.source == BLOB_SOURCE_FILE)
      {
        Blob blob = Blob::Create();
        std::vector<unsigned char> &bytes = blob.mutableBytes();
        int detailCode = SIMPLE_VIEWER_FLOW_ERROR_CODE_FILE_READ_FAILED;
        bool loaded = readBytesViaPlatform(projection, bytes, detailCode);
        if (!loaded && detailCode != SIMPLE_VIEWER_FLOW_ERROR_CODE_IMAGE_LOAD_REQUIRES_RELEASE)
        {
          loaded = readFileBytes(projection.request.filePath, bytes, detailCode);
        }
        if (!loaded)
        {
          error.kind = SIMPLE_VIEWER_FLOW_ERROR_BLOB_LOAD;
          error.code = detailCode;
          return loka::dsl::FLOW_STEP_FAILED;
        }

        blob.sealBytes();
        out = blob;
        return loka::dsl::FLOW_STEP_SUCCEEDED;
      }

      // BLOB_SOURCE_BYTES — not used in SimpleViewer but handle gracefully
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }

  private:
    virtual bool allows(std::size_t requiredBytes) const
    {
      std::size_t largestAllocation = 0;
      return !this->ctx_ || !this->ctx_->queryLargestContiguousAllocation(largestAllocation) ||
             requiredBytes <= largestAllocation;
    }

    static bool mapReadResult(loka::platform::file::ReadResult result, int &detailCodeOut)
    {
      using namespace loka::platform::file;
      switch (result)
      {
      case READ_OK: return true;
      case READ_NO_NATIVE_SPEC: detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_NO_FSSPEC; break;
      case READ_NATIVE_OPEN_FAILED: detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_OPEN_DF_FAILED; break;
      case READ_NATIVE_SIZE_FAILED: detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_GETEOF_FAILED; break;
      case READ_NATIVE_READ_FAILED: detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_READ_FAILED; break;
      case READ_STDIO_OPEN_FAILED: detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_OPEN_FAILED; break;
      case READ_STDIO_SEEK_FAILED: detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_SEEK_FAILED; break;
      case READ_STDIO_READ_FAILED: detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_READ_FAILED; break;
      case READ_CAPACITY_REFUSED: detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_IMAGE_LOAD_REQUIRES_RELEASE; break;
      case READ_SIZE_OVERFLOW: break; // The original overflow path retained the preceding detail.
      }
      return false;
    }

    bool readFileBytes(const loka::core::String &path, std::vector<unsigned char> &out, int &detailCodeOut) const
    {
      return mapReadResult(loka::platform::file::ReadBytes(path, out, this), detailCodeOut);
    }

    bool readBytesViaPlatform(const In &projection, std::vector<unsigned char> &out, int &detailCodeOut) const
    {
      out.clear();
      if (!this->ctx_ || !projection.hasFileItem)
      {
        return false;
      }
      loka::platform::file::FileHandle handle;
      if (!this->ctx_->openFile(projection.fileItem, handle))
      {
        detailCodeOut = SIMPLE_VIEWER_FLOW_ERROR_CODE_PLATFORM_OPENFILE_FAILED;
        return false;
      }
      return mapReadResult(loka::platform::file::ReadBytes(handle, out, this), detailCodeOut);
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

      // The whole blob is this file's one picture, so the range is all of it.
      if (this->ctx_->createImageFromBlob(blob, 0, blob.bytes().size(), attempt.image))
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
