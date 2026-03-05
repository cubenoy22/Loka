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

  struct ChooserToProjectionAdapter
  {
    typedef loka::app::FileChooserResult In;
    typedef ChooserProjection Out;

    loka::dsl::StepRunStatus run(const In &result, Out &projection, loka::dsl::FlowError &) const
    {
      projection = Out();
      projection.message = formatChooserMessage(result);
      if (result.kind == loka::app::FileChooserResult::RESULT_FILE)
      {
        const loka::core::String path = result.item.toString();
        if (!path.empty())
        {
          projection.request.setFilePath(path);
          projection.request.setTag(loka::core::String::Literal("image-file"));
        }
      }
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

  struct BlobToImageAdapter
  {
    typedef loka::core::resource::Blob In;
    typedef loka::core::resource::Image Out;

    explicit BlobToImageAdapter(PlatformContext *ctx) : ctx_(ctx) {}

    loka::dsl::StepRunStatus run(const In &blob, Out &image, loka::dsl::FlowError &) const
    {
      image = Out::Empty();
      if (this->ctx_ && this->ctx_->createImageFromBlob(blob, image))
      {
        return loka::dsl::FLOW_STEP_SUCCEEDED;
      }
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }

    PlatformContext *ctx_;
  };
} // namespace simpleviewer

#endif // LOKA_SIMPLE_VIEWER_FLOW_ADAPTERS_HPP
