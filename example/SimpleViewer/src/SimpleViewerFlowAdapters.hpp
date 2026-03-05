#ifndef LOKA_SIMPLE_VIEWER_FLOW_ADAPTERS_HPP
#define LOKA_SIMPLE_VIEWER_FLOW_ADAPTERS_HPP

#include "app/OpenFileDialog.hpp"
#include "app/PlatformContext.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/BlobLoader.hpp"
#include "core/resource/Image.hpp"
#include "loka/dsl/dsl.hpp"

namespace simpleviewer
{
  struct ChooserToBlobRequestAdapter
  {
    typedef loka::app::FileChooserResult In;
    typedef loka::core::resource::BlobLoaderRequest Out;

    loka::dsl::StepRunStatus run(const In &result, Out &request, loka::dsl::FlowError &) const
    {
      request = Out();
      if (result.kind == loka::app::FileChooserResult::RESULT_FILE)
      {
        const loka::core::String path = result.item.toString();
        if (!path.empty())
        {
          request.setFilePath(path);
          request.setTag(loka::core::String::Literal("image-file"));
        }
      }
      return loka::dsl::FLOW_STEP_SUCCEEDED;
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
