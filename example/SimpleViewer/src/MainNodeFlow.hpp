#ifndef LOKA_SIMPLE_VIEWER_MAIN_NODE_FLOW_HPP
#define LOKA_SIMPLE_VIEWER_MAIN_NODE_FLOW_HPP

namespace simpleviewer
{
  inline bool MainNode::IsNoFileSelectedError(const loka::dsl::FlowError &error, void *)
  {
    return error.code == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_CODE_NO_FILE_SELECTED;
  }

  inline loka::dsl::FlowHandleResult MainNode::OnBlobDecodeFailure(const loka::dsl::FlowError &error, void *userData)
  {
    MainNode *self = static_cast<MainNode *>(userData);
    if (self)
    {
      self->setEmptyImageIfNeeded();
      self->setChooserMessageIfChanged(buildErrorMessage(error));
    }
    return loka::dsl::FLOW_ERROR_HANDLED;
  }

  inline loka::dsl::FlowHandleResult MainNode::OnBlobLoadCanceled(const loka::dsl::FlowError &, void *userData)
  {
    MainNode *self = static_cast<MainNode *>(userData);
    if (!self)
    {
      return loka::dsl::FLOW_ERROR_HANDLED;
    }
    self->setEmptyImageIfNeeded();
    return loka::dsl::FLOW_ERROR_HANDLED;
  }

  inline loka::dsl::FlowHandleResult MainNode::OnBlobLoadFailure(const loka::dsl::FlowError &error, void *userData)
  {
    MainNode *self = static_cast<MainNode *>(userData);
    if (!self)
    {
      return loka::dsl::FLOW_ERROR_HANDLED;
    }
    self->setEmptyImageIfNeeded();
    self->setChooserMessageIfChanged(buildErrorMessage(error));
    return loka::dsl::FLOW_ERROR_HANDLED;
  }

  inline void MainNode::OnChooserProjection(const simpleviewer::ChooserProjection &projection, void *userData)
  {
    MainNode *self = static_cast<MainNode *>(userData);
    if (self)
    {
      self->chooserMessage_.set(projection.message, true);
    }
  }

  inline void MainNode::OnImageDecoded(const loka::core::resource::Image &image, void *userData)
  {
    MainNode *self = static_cast<MainNode *>(userData);
    if (self)
    {
      self->image_.set(image, true);
    }
  }

  // MainNode's file-chooser pipeline starts here.
  // This is the main entry point in this file; the handlers above support it.
  inline MainNode::ViewerFlowChain MainNode::buildFlow(MainNode &self)
  {
    ViewerFlowChain chain =
        loka::dsl::Flow() //
        | loka::dsl::Step(1, simpleviewer::ChooserToContextAdapter())
        | loka::dsl::Step(2, simpleviewer::ContextToProjectionAdapter())
              .onSuccess(&MainNode::OnChooserProjection, &self)
        | loka::dsl::Step(3, simpleviewer::ProjectionToBlobAdapter(self.props.platformContext_))
              .onFailure(&MainNode::IsNoFileSelectedError, &MainNode::OnBlobLoadCanceled, &self)
              .onFailure(&MainNode::OnBlobLoadFailure, &self)
        | loka::dsl::Step(4, simpleviewer::BlobToDecodeAttemptAdapter(self.props.platformContext_))
              .onFailure(&MainNode::OnBlobDecodeFailure, &self)
        | loka::dsl::Step(5, simpleviewer::DecodeAttemptToImageAdapter()).onSuccess(&MainNode::OnImageDecoded, &self);
    return chain;
  }

  inline loka::core::String MainNode::buildErrorMessage(const loka::dsl::FlowError &error)
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
    if (error.code == SIMPLE_VIEWER_FLOW_ERROR_CODE_PATH_UTF8_CONVERT_FAILED)
    {
      return loka::core::String::Literal("Read failed: path UTF-8 conversion.");
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
    return loka::core::String::Literal("Unexpected flow error code: ") + loka::core::String::FromInt(error.code);
  }
} // namespace simpleviewer

#endif // LOKA_SIMPLE_VIEWER_MAIN_NODE_FLOW_HPP
