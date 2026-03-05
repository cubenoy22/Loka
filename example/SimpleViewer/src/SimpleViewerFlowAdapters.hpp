#ifndef LOKA_SIMPLE_VIEWER_FLOW_ADAPTERS_HPP
#define LOKA_SIMPLE_VIEWER_FLOW_ADAPTERS_HPP

#include "app/OpenFileDialog.hpp"
#include "app/PlatformContext.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/BlobLoader.hpp"
#include "core/resource/Image.hpp"
#include "loka/core/String.hpp"
#include "loka/dsl/dsl.hpp"

namespace simpleviewer
{
  enum SimpleViewerFlowErrorKind
  {
    SIMPLE_VIEWER_FLOW_ERROR_NONE = 0,
    SIMPLE_VIEWER_FLOW_ERROR_DECODE = 1
  };

  enum SimpleViewerFlowErrorCode
  {
    SIMPLE_VIEWER_FLOW_ERROR_CODE_PLATFORM_CONTEXT_MISSING = 1001,
    SIMPLE_VIEWER_FLOW_ERROR_CODE_IMAGE_DECODE_FAILED = 1002
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
