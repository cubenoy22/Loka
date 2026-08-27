#ifndef LOKA_SIMPLE_VIEWER_IMAGE_LOAD_SESSION_FLOW_HPP
#define LOKA_SIMPLE_VIEWER_IMAGE_LOAD_SESSION_FLOW_HPP

namespace simpleviewer
{
  inline void ImageLoadSession::begin(MainNode &owner,
                                      PlatformContext *platformContext,
                                      loka::core::State<loka::app::FileChooserResult> *trigger,
                                      loka::core::PushStateTracker *tracker)
  {
    assert(platformContext != 0 && "ImageLoadSession::begin requires a platform context");
    assert(trigger != 0 && "ImageLoadSession::begin requires a chooser-result trigger");
    assert(tracker != 0 && "ImageLoadSession::begin requires its Boundary tracker");
    this->flow_.clear();
    this->owner_ = &owner;
    this->platformContext_ = platformContext;
    this->flow_.set(this->buildFlow()).bindTrigger(trigger).withTracker(tracker);
  }

  inline bool ImageLoadSession::IsNoFileSelectedError(const loka::dsl::FlowError &error, void *)
  {
    return error.code == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_CODE_NO_FILE_SELECTED;
  }

  inline bool ImageLoadSession::CanReleaseCurrentImageForLoad(const loka::dsl::FlowError &error, void *userData)
  {
    ImageLoadSession *self = static_cast<ImageLoadSession *>(userData);
    return self && self->owner_ &&
           error.code == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_CODE_IMAGE_LOAD_REQUIRES_RELEASE &&
           self->owner_->hasCurrentImage();
  }

  inline loka::dsl::FlowHandleResult
  ImageLoadSession::OnBlobDecodeFailure(const loka::dsl::FlowError &error, void *userData)
  {
    ImageLoadSession *self = static_cast<ImageLoadSession *>(userData);
    if (self && self->owner_)
    {
      self->owner_->setChooserMessageIfChanged(buildErrorMessage(error));
    }
    return loka::dsl::FLOW_ERROR_HANDLED;
  }

  inline loka::dsl::FlowHandleResult
  ImageLoadSession::OnCurrentImageReleasedForLoad(const loka::dsl::FlowError &, void *userData)
  {
    ImageLoadSession *self = static_cast<ImageLoadSession *>(userData);
    if (self && self->owner_)
    {
      self->owner_->releaseCurrentImageForLoad();
    }
    return loka::dsl::FLOW_ERROR_HANDLED;
  }

  inline loka::dsl::FlowHandleResult
  ImageLoadSession::OnBlobLoadCanceled(const loka::dsl::FlowError &, void *userData)
  {
    (void)userData;
    return loka::dsl::FLOW_ERROR_HANDLED;
  }

  inline loka::dsl::FlowHandleResult
  ImageLoadSession::OnBlobLoadFailure(const loka::dsl::FlowError &error, void *userData)
  {
    ImageLoadSession *self = static_cast<ImageLoadSession *>(userData);
    if (self && self->owner_)
    {
      self->owner_->setChooserMessageIfChanged(buildErrorMessage(error));
    }
    return loka::dsl::FLOW_ERROR_HANDLED;
  }

  inline void ImageLoadSession::OnChooserCompletion(const simpleviewer::ChooserContext &context, void *userData)
  {
    ImageLoadSession *self = static_cast<ImageLoadSession *>(userData);
    if (self && self->owner_)
    {
      self->owner_->closeDialogForChooserResult(context.result);
    }
  }

  inline void ImageLoadSession::OnChooserProjection(const simpleviewer::ChooserProjection &projection,
                                                     void *userData)
  {
    ImageLoadSession *self = static_cast<ImageLoadSession *>(userData);
    if (self && self->owner_)
    {
      self->owner_->setChooserMessageIfChanged(projection.message);
    }
  }

  inline void ImageLoadSession::OnImageLoaded(const loka::core::resource::Image &image, void *userData)
  {
    ImageLoadSession *self = static_cast<ImageLoadSession *>(userData);
    if (self && self->owner_)
    {
      self->owner_->commitLoadedImage(image);
    }
  }

  inline void ImageLoadSession::OnImageLoadFinished(void *userData)
  {
    ImageLoadSession *self = static_cast<ImageLoadSession *>(userData);
    if (self && self->owner_ && !self->owner_->isImageLoadDialogShown())
    {
      self->finish();
    }
  }

  inline ImageLoadSession::Flow ImageLoadSession::buildFlow()
  {
    Flow chain =
        loka::dsl::Flow() //
        | loka::dsl::Step(1, simpleviewer::ChooserToContextAdapter())
              .onSuccess(&ImageLoadSession::OnChooserCompletion, this)
        | loka::dsl::Step(2, simpleviewer::ContextToProjectionAdapter())
              .onSuccess(&ImageLoadSession::OnChooserProjection, this)
        | loka::dsl::Step(3, simpleviewer::ProjectionToBlobAdapter(this->platformContext_))
              .onFailure(&ImageLoadSession::CanReleaseCurrentImageForLoad,
                         &ImageLoadSession::OnCurrentImageReleasedForLoad,
                         this,
                         3)
              .onFailure(&ImageLoadSession::IsNoFileSelectedError, &ImageLoadSession::OnBlobLoadCanceled, this)
              .onFailure(&ImageLoadSession::OnBlobLoadFailure, this)
        | loka::dsl::Step(4, simpleviewer::BlobToDecodeAttemptAdapter(this->platformContext_))
              .onFailure(&ImageLoadSession::OnBlobDecodeFailure, this)
        | loka::dsl::Step(5, simpleviewer::DecodeAttemptToImageAdapter())
              .onSuccess(&ImageLoadSession::OnImageLoaded, this);
    chain.onFinally(&ImageLoadSession::OnImageLoadFinished, this);
    return chain;
  }

  inline loka::core::String ImageLoadSession::buildErrorMessage(const loka::dsl::FlowError &error)
  {
    using namespace simpleviewer;
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_PLATFORM_CONTEXT_MISSING)
    {
      return loka::core::String::Literal("Platform context is missing.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_FILE_READ_FAILED)
    {
      return loka::core::String::Literal("Failed to read file.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_PLATFORM_OPENFILE_FAILED)
    {
      return loka::core::String::Literal("Read failed: platform openFile failed.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_NO_FSSPEC)
    {
      return loka::core::String::Literal("Read failed: Classic FSSpec missing.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_OPEN_DF_FAILED)
    {
      return loka::core::String::Literal("Read failed: FSpOpenDF failed.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_GETEOF_FAILED)
    {
      return loka::core::String::Literal("Read failed: GetEOF failed.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_CLASSIC_READ_FAILED)
    {
      return loka::core::String::Literal("Read failed: FSRead failed.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_OPEN_FAILED)
    {
      return loka::core::String::Literal("Read failed: fopen failed.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_SEEK_FAILED)
    {
      return loka::core::String::Literal("Read failed: fseek failed.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_READ_FAILED)
    {
      return loka::core::String::Literal("Read failed: fread failed.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_NO_FILE_SELECTED)
    {
      return loka::core::String::Literal("No file selected.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_IMAGE_DECODE_FAILED)
    {
      return loka::core::String::Literal("Failed to decode image. Classic supports mainly uncompressed PICT; "
                                         "QuickTime-compressed PICT or low memory may fail.");
    }
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_IMAGE_LOAD_REQUIRES_RELEASE)
    {
      return loka::core::String::Literal("Not enough contiguous memory to load image.");
    }
    return loka::core::String::Literal("Unexpected flow error code: ") + loka::core::String::FromInt(error.code);
  }

  inline void ImageLoadSession::finish()
  {
    this->flow_.clear();
    this->owner_ = 0;
    this->platformContext_ = 0;
  }
} // namespace simpleviewer

#endif // LOKA_SIMPLE_VIEWER_IMAGE_LOAD_SESSION_FLOW_HPP
