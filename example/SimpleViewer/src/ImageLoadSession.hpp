#ifndef LOKA_SIMPLE_VIEWER_IMAGE_LOAD_SESSION_HPP
#define LOKA_SIMPLE_VIEWER_IMAGE_LOAD_SESSION_HPP

#include <cassert>

#include "app/OpenFileDialog.hpp"
#include "app/PlatformContext.hpp"
#include "app/scene/state/FlowSlot.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/String.hpp"
#include "core/resource/Image.hpp"
#include "dsl/flow/Flow.hpp"
#include "SimpleViewerFlowAdapters.hpp"

namespace simpleviewer
{
  class MainNode;

  /** Owns the temporary Flow residents for one image-load attempt.
      MainNode owns the displayed Image; this session owns only the candidate
      until it is committed or the attempt reaches a terminal result. */
  class ImageLoadSession
  {
  public:
    typedef loka::dsl::FlowChain<loka::app::FileChooserResult, loka::core::resource::Image> Flow;

    ImageLoadSession()
        : owner_(0),
          platformContext_(0),
          flow_()
    {
    }

    void begin(MainNode &owner,
               PlatformContext *platformContext,
               loka::core::State<loka::app::FileChooserResult> *trigger,
               loka::core::PushStateTracker *tracker);

  private:
    static bool IsNoFileSelectedError(const loka::dsl::FlowError &error, void *);
    static bool CanReleaseCurrentImageForLoad(const loka::dsl::FlowError &error, void *userData);
    static loka::dsl::FlowHandleResult OnBlobDecodeFailure(const loka::dsl::FlowError &error, void *userData);
    static loka::dsl::FlowHandleResult OnCurrentImageReleasedForLoad(const loka::dsl::FlowError &error,
                                                                     void *userData);
    static loka::dsl::FlowHandleResult OnBlobLoadCanceled(const loka::dsl::FlowError &error, void *userData);
    static loka::dsl::FlowHandleResult OnBlobLoadFailure(const loka::dsl::FlowError &error, void *userData);
    static void OnChooserCompletion(const simpleviewer::ChooserContext &context, void *userData);
    static void OnChooserProjection(const simpleviewer::ChooserProjection &projection, void *userData);
    static void OnImageLoaded(const loka::core::resource::Image &image, void *userData);
    static void OnImageLoadFinished(void *userData);

    Flow buildFlow();
    static loka::core::String buildErrorMessage(const loka::dsl::FlowError &error);
    void finish();

    MainNode *owner_;
    PlatformContext *platformContext_;
    loka::app::scene::FlowSlot<Flow> flow_;

    ImageLoadSession(const ImageLoadSession &);
    ImageLoadSession &operator=(const ImageLoadSession &);
  };
} // namespace simpleviewer

#endif // LOKA_SIMPLE_VIEWER_IMAGE_LOAD_SESSION_HPP
