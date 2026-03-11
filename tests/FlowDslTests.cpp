#include "FlowDslTests.hpp"

#include <cassert>
#include <cstdio>
#include <vector>

#include "app/Box.hpp"
#include "app/Button.hpp"
#include "app/Text.hpp"
#include "app/scene/NodeComposition.hpp"
#include "app/scene/PlatformController.hpp"
#include "app/scene/Scene.hpp"
#include "../example/SimpleViewer/src/SimpleViewerFlowAdapters.hpp"
#include "loka/core/State.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "loka/dsl/testing/SceneTestFlow.hpp"
#include "loka/dsl/dsl.hpp"

namespace {
  struct FlowTestMarkerContext {
    std::vector<int> *order;
    int marker;
  };

  struct FlowErrorCapture {
    int kind;
    int code;
    int calls;
  };

  struct FlowTestMarker {
    static void onStepSuccess(const int &, void *user) {
      FlowTestMarkerContext *ctx = static_cast<FlowTestMarkerContext *>(user);
      ctx->order->push_back(ctx->marker);
    }

    static void onStepFinally(void *user) {
      FlowTestMarkerContext *ctx = static_cast<FlowTestMarkerContext *>(user);
      ctx->order->push_back(ctx->marker);
    }

    static void onFlowSuccess(void *user) {
      FlowTestMarkerContext *ctx = static_cast<FlowTestMarkerContext *>(user);
      ctx->order->push_back(ctx->marker);
    }

    static loka::dsl::FlowHandleResult onFlowFailureHandled(const loka::dsl::FlowError &, void *user) {
      FlowTestMarkerContext *ctx = static_cast<FlowTestMarkerContext *>(user);
      ctx->order->push_back(ctx->marker);
      return loka::dsl::FLOW_ERROR_HANDLED;
    }

    static loka::dsl::FlowHandleResult onStepFailureUnhandled(const loka::dsl::FlowError &, void *user) {
      FlowTestMarkerContext *ctx = static_cast<FlowTestMarkerContext *>(user);
      ctx->order->push_back(ctx->marker);
      return loka::dsl::FLOW_ERROR_UNHANDLED;
    }

    static loka::dsl::FlowHandleResult onStepFailureHandled(const loka::dsl::FlowError &, void *user) {
      FlowTestMarkerContext *ctx = static_cast<FlowTestMarkerContext *>(user);
      ctx->order->push_back(ctx->marker);
      return loka::dsl::FLOW_ERROR_HANDLED;
    }

    static bool is500(const loka::dsl::FlowError &error, void *) {
      return error.code == 500;
    }

    static loka::dsl::FlowHandleResult captureFailure(const loka::dsl::FlowError &error, void *user) {
      FlowErrorCapture *capture = static_cast<FlowErrorCapture *>(user);
      if (capture) {
        capture->kind = error.kind;
        capture->code = error.code;
        ++capture->calls;
      }
      return loka::dsl::FLOW_ERROR_HANDLED;
    }
  };

  struct FlowTestMul2Adapter {
    typedef int In;
    typedef int Out;
    loka::dsl::StepRunStatus run(const int &in, int &out, loka::dsl::FlowError &) const {
      out = in * 2;
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }
  };

  struct FlowTestAdd1Adapter {
    typedef int In;
    typedef int Out;
    loka::dsl::StepRunStatus run(const int &in, int &out, loka::dsl::FlowError &) const {
      out = in + 1;
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }
  };

  struct FlowTestFail500Adapter {
    typedef int In;
    typedef int Out;
    loka::dsl::StepRunStatus run(const int &, int &out, loka::dsl::FlowError &error) const {
      out = 0;
      error.kind = 1;
      error.code = 500;
      return loka::dsl::FLOW_STEP_FAILED;
    }
  };

  struct FlowTestCheckLoadingAdapter {
    typedef int In;
    typedef int Out;

    explicit FlowTestCheckLoadingAdapter(const bool *loadingState) : loadingState_(loadingState) {
    }

    loka::dsl::StepRunStatus run(const int &in, int &out, loka::dsl::FlowError &) const {
      assert(this->loadingState_ != 0);
      assert(*this->loadingState_ == true);
      out = in;
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }

    const bool *loadingState_;
  };

  struct FlowTestPendingThenSuccessAdapter {
    typedef int In;
    typedef int Out;

    FlowTestPendingThenSuccessAdapter(bool *ready, int *calls) : ready_(ready), calls_(calls) {
    }

    loka::dsl::StepRunStatus run(const int &in, int &out, loka::dsl::FlowError &) const {
      ++(*this->calls_);
      if (!*this->ready_) {
        return loka::dsl::FLOW_STEP_PENDING;
      }
      out = in + 100;
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }

    bool *ready_;
    int *calls_;
  };

  struct FlowTestFailOnceAdapter {
    typedef int In;
    typedef int Out;

    explicit FlowTestFailOnceAdapter(bool *failedOnce) : failedOnce_(failedOnce) {
    }

    loka::dsl::StepRunStatus run(const int &in, int &out,
                                 loka::dsl::FlowError &error) const {
      if (!*this->failedOnce_) {
        *this->failedOnce_ = true;
        error.kind = 9;
        error.code = 500;
        return loka::dsl::FLOW_STEP_FAILED;
      }
      out = in + 7;
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }

    bool *failedOnce_;
  };

  struct FlowTestCountedPassAdapter {
    typedef int In;
    typedef int Out;

    explicit FlowTestCountedPassAdapter(int *calls) : calls_(calls) {
    }

    loka::dsl::StepRunStatus run(const int &in, int &out,
                                 loka::dsl::FlowError &) const {
      ++(*this->calls_);
      out = in + 1;
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }

    int *calls_;
  };

  struct FlowTestFieldOutput {
    int value;
    int extra;
    FlowTestFieldOutput() : value(0), extra(0) {}
  };

  struct FlowTestFieldAdapter {
    typedef int In;
    typedef FlowTestFieldOutput Out;
    loka::dsl::StepRunStatus run(const int &in, FlowTestFieldOutput &out, loka::dsl::FlowError &) const {
      out.value = in * 10;
      out.extra = in + 1;
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }
  };

  struct FlowTestPlatformContext : public PlatformContext {
    FlowTestPlatformContext()
        : createImageResult_(false),
          createImageCalls_(0),
          width_(16),
          height_(16) {
    }

    virtual App *createApp(AppConfigurable *, HINSTANCE, int) const { return 0; }
    virtual Window *createWindow(const WindowProps &) { return 0; }
    virtual loka::app::scene::NodeContext *createNodeContext(loka::app::scene::Node *) const { return 0; }
    virtual bool openFile(const loka::file::File &, loka::platform::file::FileHandle &) const { return false; }
    virtual bool createImageFromBlob(const loka::core::resource::Blob &,
                                     loka::core::resource::Image &out) const {
      ++createImageCalls_;
      if (!createImageResult_) {
        return false;
      }
      out = loka::core::resource::Image::FromNative(reinterpret_cast<void *>(0x1), width_, height_,
                                                    &FlowTestPlatformContext::ReleaseNativeNoop, 0);
      return true;
    }

    static void ReleaseNativeNoop(void *, void *) {}

    bool createImageResult_;
    mutable int createImageCalls_;
    int width_;
    int height_;
  };

  struct StateNotifyDeleteCtx {
    loka::core::EmitterState *state;
    loka::core::MutableState<int> *valueState;
    int destroyCalls;
    int siblingCalls;
  };

  struct StateNotifyHelpers {
    static void destroySelf(void *user) {
      StateNotifyDeleteCtx *ctx = static_cast<StateNotifyDeleteCtx *>(user);
      ++ctx->destroyCalls;
      if (ctx->state) {
        loka::core::EmitterState *owned = ctx->state;
        ctx->state = 0;
        delete owned;
      }
    }

    static void sibling(void *user) {
      StateNotifyDeleteCtx *ctx = static_cast<StateNotifyDeleteCtx *>(user);
      ++ctx->siblingCalls;
    }

    static void destroyValueSelf(void *user) {
      StateNotifyDeleteCtx *ctx = static_cast<StateNotifyDeleteCtx *>(user);
      ++ctx->destroyCalls;
      if (ctx->valueState) {
        loka::core::MutableState<int> *owned = ctx->valueState;
        ctx->valueState = 0;
        delete owned;
      }
    }

    static void selfUnbind(void *user) {
      StateNotifyDeleteCtx *ctx = static_cast<StateNotifyDeleteCtx *>(user);
      ++ctx->destroyCalls;
      if (ctx->state) {
        ctx->state->unbind(&StateNotifyHelpers::selfUnbind, user);
      }
    }

    static void unbindSibling(void *user) {
      StateNotifyDeleteCtx *ctx = static_cast<StateNotifyDeleteCtx *>(user);
      ++ctx->destroyCalls;
      if (ctx->state) {
        ctx->state->unbind(&StateNotifyHelpers::sibling, user);
      }
    }

    static void unbindValueSibling(void *user) {
      StateNotifyDeleteCtx *ctx = static_cast<StateNotifyDeleteCtx *>(user);
      ++ctx->destroyCalls;
      if (ctx->valueState) {
        ctx->valueState->unbind(&StateNotifyHelpers::sibling, user);
      }
    }
  };

  struct FlowScenePlatformController : public loka::app::scene::IPlatformController {
    FlowScenePlatformController() : lastMaterialized_(0), lastFlags_(loka::app::scene::NODE_DIRTY_NONE), destroyed_(false) {
    }

    virtual void onChange(loka::app::scene::Node *rootNode, loka::app::scene::NodeDirtyFlags flags) {
      lastMaterialized_ = rootNode;
      lastFlags_ = flags;
    }

    virtual void synchronize() {
    }

    virtual void destroy() {
      destroyed_ = true;
    }

    loka::app::scene::Node *lastMaterialized_;
    loka::app::scene::NodeDirtyFlags lastFlags_;
    bool destroyed_;
  };
} // namespace

void testLokaFlowDslV1Core() {
  printf("\n==== [testLokaFlowDslV1Core] start ====\n");

  {
    int input = 3;
    int captured = 0;
    std::vector<int> order;

    FlowTestMarkerContext s1 = {&order, 10};
    FlowTestMarkerContext f1 = {&order, 11};
    FlowTestMarkerContext fs1 = {&order, 12};
    FlowTestMarkerContext s2 = {&order, 20};
    FlowTestMarkerContext f2 = {&order, 21};
    FlowTestMarkerContext fs2 = {&order, 22};
    FlowTestMarkerContext ff = {&order, 99};

    loka::dsl::FlowChain<int, int> chain = //
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestMul2Adapter())
              .input(&input)
              .onSuccess(&captured)
              .onSuccess(&FlowTestMarker::onStepSuccess, &s1)
              .onFinally(&FlowTestMarker::onStepFinally, &f1)
        | loka::dsl::Step(2, FlowTestAdd1Adapter())
              .onSuccess(&FlowTestMarker::onStepSuccess, &s2)
              .onFinally(&FlowTestMarker::onStepFinally, &f2);
    chain.onSuccess(&FlowTestMarker::onFlowSuccess, &fs1);
    chain.onSuccess(&FlowTestMarker::onFlowSuccess, &fs2);
    chain.onFinally(&FlowTestMarker::onStepFinally, &ff);

    const bool ok = chain.run();
    assert(ok);
    assert(captured == 6);
    assert(order.size() == 7);
    assert(order[0] == 10);
    assert(order[1] == 11);
    assert(order[2] == 20);
    assert(order[3] == 21);
    assert(order[4] == 12);
    assert(order[5] == 22);
    assert(order[6] == 99);
  }

  {
    int input = 7;
    std::vector<int> order;

    FlowTestMarkerContext s1 = {&order, 110};
    FlowTestMarkerContext f1 = {&order, 111};
    FlowTestMarkerContext fs = {&order, 112};
    FlowTestMarkerContext failMatchUnhandled = {&order, 130};
    FlowTestMarkerContext failDefaultHandled = {&order, 131};
    FlowTestMarkerContext f2 = {&order, 132};
    FlowTestMarkerContext s3 = {&order, 140};
    FlowTestMarkerContext flowFail = {&order, 150};
    FlowTestMarkerContext flowFinal = {&order, 199};

    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestMul2Adapter())
              .input(&input)
              .onSuccess(&FlowTestMarker::onStepSuccess, &s1)
              .onFinally(&FlowTestMarker::onStepFinally, &f1)
        | loka::dsl::Step(2, FlowTestFail500Adapter())
              .onFailure(&FlowTestMarker::is500, &FlowTestMarker::onStepFailureUnhandled, &failMatchUnhandled)
              .onFailure(&FlowTestMarker::onStepFailureHandled, &failDefaultHandled)
              .onFinally(&FlowTestMarker::onStepFinally, &f2)
        | loka::dsl::Step(3, FlowTestAdd1Adapter()).onSuccess(&FlowTestMarker::onStepSuccess, &s3);
    chain.onSuccess(&FlowTestMarker::onFlowSuccess, &fs);
    chain.onFailure(&FlowTestMarker::onFlowFailureHandled, &flowFail);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const bool ok = chain.run();
    assert(ok);
    assert(order.size() == 6);
    assert(order[0] == 110);
    assert(order[1] == 111);
    assert(order[2] == 130);
    assert(order[3] == 132);
    assert(order[4] == 150);
    assert(order[5] == 199);

    // Flow failure handler must run, default step failure handler must not run
    // (first-match-wins).
    bool hasFlowFailure = false;
    bool hasDefaultStepFailure = false;
    bool hasStep3Success = false;
    for (std::size_t i = 0; i < order.size(); ++i) {
      if (order[i] == 150)
        hasFlowFailure = true;
      if (order[i] == 131)
        hasDefaultStepFailure = true;
      if (order[i] == 140)
        hasStep3Success = true;
    }
    assert(hasFlowFailure);
    assert(!hasDefaultStepFailure);
    assert(!hasStep3Success);
  }

  {
    int inputA = 5;
    int inputB = 41;
    int captured = 0;
    std::vector<int> order;

    FlowTestMarkerContext s1 = {&order, 210};
    FlowTestMarkerContext f1 = {&order, 211};
    FlowTestMarkerContext s2 = {&order, 220};
    FlowTestMarkerContext f2 = {&order, 221};
    FlowTestMarkerContext flowFinal = {&order, 299};

    loka::dsl::FlowChain<int, int> chain = //
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestMul2Adapter())
              .input(&inputA)
              .onSuccess(&FlowTestMarker::onStepSuccess, &s1)
              .onFinally(&FlowTestMarker::onStepFinally, &f1)
        | loka::dsl::Step(2, FlowTestAdd1Adapter())
              .input(&inputB)
              .onSuccess(&captured)
              .onSuccess(&FlowTestMarker::onStepSuccess, &s2)
              .onFinally(&FlowTestMarker::onStepFinally, &f2);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const bool resumedOk = chain.resume(2);
    assert(resumedOk);
    assert(captured == 42);
    assert(order.size() == 3);
    assert(order[0] == 220);
    assert(order[1] == 221);
    assert(order[2] == 299);

    const bool missing = chain.resume(9999);
    assert(!missing);
  }

  {
    int input = 10;
    bool loading = false;
    std::vector<int> order;
    FlowTestMarkerContext flowFinal = {&order, 399};

    // Keep this chain multi-line for readability of the DSL flow.
    loka::dsl::FlowChain<int, int> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, FlowTestCheckLoadingAdapter(&loading))
              .input(&input);
    chain.trackLoading(&loading);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    assert(loading == false);
    const bool ok = chain.run();
    assert(ok);
    assert(loading == false);
    assert(order.size() == 1);
    assert(order[0] == 399);
  }

  {
    int input = 9;
    bool loading = false;
    std::vector<int> order;
    FlowTestMarkerContext flowFinal = {&order, 499};

    loka::dsl::FlowChain<int, int> chain = //
        loka::dsl::Flow()                  //
        | loka::dsl::Step(1, FlowTestFail500Adapter()).input(&input);
    chain.trackLoading(&loading);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    assert(loading == false);
    const bool ok = chain.run();
    assert(!ok);
    assert(loading == false);
    assert(order.size() == 1);
    assert(order[0] == 499);
  }

  {
    int input = 23;
    int calls = 0;
    bool ready = false;
    bool loading = false;
    int captured = 0;
    std::vector<int> order;
    FlowTestMarkerContext stepFinal = {&order, 510};
    FlowTestMarkerContext flowFinal = {&order, 599};

    loka::dsl::FlowChain<int, int> chain = loka::dsl::Flow()
                                           | loka::dsl::Step(1, FlowTestPendingThenSuccessAdapter(&ready, &calls))
                                                 .input(&input)
                                                 .onSuccess(&captured)
                                                 .onFinally(&FlowTestMarker::onStepFinally, &stepFinal);
    chain.trackLoading(&loading);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const loka::dsl::FlowRunResult first = chain.runResult();
    assert(first == loka::dsl::FLOW_RUN_PENDING);
    assert(calls == 1);
    assert(loading == true);
    assert(order.empty()); // no finally on pending

    ready = true;
    const loka::dsl::FlowRunResult resumed = chain.resumeResult(1);
    assert(resumed == loka::dsl::FLOW_RUN_SUCCEEDED);
    assert(calls == 2);
    assert(captured == 123);
    assert(loading == false);
    assert(order.size() == 2);
    assert(order[0] == 510);
    assert(order[1] == 599);
  }

  {
    int input = 31;
    int calls = 0;
    bool ready = false;
    int captured = 0;
    std::vector<int> order;
    FlowTestMarkerContext flowFinal = {&order, 605};

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestPendingThenSuccessAdapter(&ready, &calls))
                .input(&input)
                .onSuccess(&captured);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const loka::dsl::FlowRunResult first = chain.runResult();
    assert(first == loka::dsl::FLOW_RUN_PENDING);
    assert(calls == 1);
    assert(captured == 0);
    assert(order.empty());

    chain.cancel();
    ready = true;
    const loka::dsl::FlowRunResult canceled = chain.resumeResult(1);
    assert(canceled == loka::dsl::FLOW_RUN_CANCELED);
    assert(calls == 1);
    assert(captured == 0);
    assert(order.size() == 1);
    assert(order[0] == 605);
  }

  {
    int input = 41;
    int calls = 0;
    bool ready = false;
    int captured = 0;
    std::vector<int> order;
    FlowTestMarkerContext flowFinal = {&order, 615};

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestPendingThenSuccessAdapter(&ready, &calls))
                .input(&input)
                .onSuccess(&captured);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    assert(chain.runResult() == loka::dsl::FLOW_RUN_PENDING);
    assert(calls == 1);

    chain.cancel();
    chain.clearCancel();
    ready = true;

    const loka::dsl::FlowRunResult resumed = chain.resumeResult(1);
    assert(resumed == loka::dsl::FLOW_RUN_SUCCEEDED);
    assert(calls == 2);
    assert(captured == 141);
    assert(order.size() == 1);
    assert(order[0] == 615);
  }

  {
    int input = 11;
    int out = 0;
    std::vector<int> order;
    FlowTestMarkerContext flowFinal = {&order, 625};

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestAdd1Adapter())
                .input(&input)
                .onSuccess(&out);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);
    chain.cancel();

    const loka::dsl::FlowRunResult canceled = chain.runResult();
    assert(canceled == loka::dsl::FLOW_RUN_CANCELED);
    assert(out == 0);
    assert(order.size() == 1);
    assert(order[0] == 625);
  }

  {
    int input = 2;
    int out = 0;
    bool failedOnce = false;
    std::vector<int> order;
    FlowTestMarkerContext stepFail = {&order, 610};
    FlowTestMarkerContext stepSuccess = {&order, 611};
    FlowTestMarkerContext flowFinal = {&order, 699};

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestFailOnceAdapter(&failedOnce))
                .input(&input)
                .onFailure(&FlowTestMarker::is500, &FlowTestMarker::onStepFailureHandled, &stepFail, 1)
                .onSuccess(&out)
                .onSuccess(&FlowTestMarker::onStepSuccess, &stepSuccess);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const bool ok = chain.run();
    assert(ok);
    assert(out == 9);
    assert(order.size() == 3);
    assert(order[0] == 610);
    assert(order[1] == 611);
    assert(order[2] == 699);
  }

  {
    int input = 3;
    int out = 0;
    bool failedOnce = false;
    std::vector<int> order;
    FlowTestMarkerContext flowFail = {&order, 710};
    FlowTestMarkerContext flowSuccess = {&order, 711};
    FlowTestMarkerContext flowFinal = {&order, 799};

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestFailOnceAdapter(&failedOnce))
                .input(&input)
                .onSuccess(&out);
    chain.onFailure(&FlowTestMarker::onFlowFailureHandled, &flowFail, 1);
    chain.onSuccess(&FlowTestMarker::onFlowSuccess, &flowSuccess);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const bool ok = chain.run();
    assert(ok);
    assert(out == 10);
    assert(order.size() == 3);
    assert(order[0] == 710);
    assert(order[1] == 711);
    assert(order[2] == 799);
  }

  {
    int input = 10;
    int out = 0;
    int callsA = 0;
    int callsB = 0;
    std::vector<int> order;
    FlowTestMarkerContext flowFinal = {&order, 899};

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestCountedPassAdapter(&callsA))
                .input(&input)
                .onSuccess(&out, 2)
          | loka::dsl::Step(2, FlowTestCountedPassAdapter(&callsB))
                .onSuccess(&out);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const bool ok = chain.run();
    assert(ok);
    assert(callsA == 1);
    assert(callsB == 1);
    assert(out == 12);
    assert(order.size() == 1);
    assert(order[0] == 899);
  }

  {
    int input = 20;
    int out = 0;
    std::vector<int> flowMarks;
    FlowTestMarkerContext flowSuccess = {&flowMarks, 1};
    std::vector<int> order;
    FlowTestMarkerContext flowFinal = {&order, 999};

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestAdd1Adapter())
                .input(&input)
                .onSuccess(&out);
    chain.onSuccess(&FlowTestMarker::onFlowSuccess, &flowSuccess, 9999);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const bool ok = chain.run();
    assert(!ok);
    assert(flowMarks.size() == 1);
    assert(out == 21);
    assert(order.size() == 1);
    assert(order[0] == 999);
  }

  {
    int input = 4;
    std::vector<int> order;
    FlowTestMarkerContext step1Final = {&order, 1001};
    FlowTestMarkerContext step2Final = {&order, 1002};
    FlowTestMarkerContext flowFinal = {&order, 1099};

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestMul2Adapter())
                .input(&input)
                .onFinally(&FlowTestMarker::onStepFinally, &step1Final)
          | loka::dsl::Step(2, FlowTestAdd1Adapter())
                .onFinally(&FlowTestMarker::onStepFinally, &step2Final);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    assert(chain.run());
    assert(order.size() == 3);
    assert(order[0] == 1001);
    assert(order[1] == 1002);
    assert(order[2] == 1099);
  }

  {
    int input = 5;
    std::vector<int> order;
    FlowTestMarkerContext step1Final = {&order, 1101};
    FlowTestMarkerContext step2Final = {&order, 1102};
    FlowTestMarkerContext step3Final = {&order, 1103};
    FlowTestMarkerContext flowFinal = {&order, 1199};

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestMul2Adapter())
                .input(&input)
                .onFinally(&FlowTestMarker::onStepFinally, &step1Final)
          | loka::dsl::Step(2, FlowTestFail500Adapter())
                .onFinally(&FlowTestMarker::onStepFinally, &step2Final)
          | loka::dsl::Step(3, FlowTestAdd1Adapter())
                .onFinally(&FlowTestMarker::onStepFinally, &step3Final);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    assert(!chain.run());
    assert(order.size() == 3);
    assert(order[0] == 1101);
    assert(order[1] == 1102);
    assert(order[2] == 1199);
  }

  {
    int input = 6;
    std::vector<int> order;
    FlowTestMarkerContext stepFirstMatch = {&order, 1201};
    FlowTestMarkerContext stepDefault = {&order, 1202};
    FlowTestMarkerContext flowFinal = {&order, 1299};

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestFail500Adapter())
                .input(&input)
                .onFailure(&FlowTestMarker::is500, &FlowTestMarker::onStepFailureUnhandled, &stepFirstMatch)
                .onFailure(&FlowTestMarker::onStepFailureHandled, &stepDefault);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    assert(!chain.run());
    assert(order.size() == 2);
    assert(order[0] == 1201);
    assert(order[1] == 1299);
  }

  {
    int input = 8;
    std::vector<int> order;
    FlowTestMarkerContext flowFirstMatch = {&order, 1301};
    FlowTestMarkerContext flowDefault = {&order, 1302};
    FlowTestMarkerContext flowFinal = {&order, 1399};

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestFail500Adapter()).input(&input);
    chain.onFailure(&FlowTestMarker::is500, &FlowTestMarker::onStepFailureUnhandled, &flowFirstMatch);
    chain.onFailure(&FlowTestMarker::onFlowFailureHandled, &flowDefault);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    assert(!chain.run());
    assert(order.size() == 2);
    assert(order[0] == 1301);
    assert(order[1] == 1399);
  }

  {
    int input = 9;
    std::vector<int> order;
    FlowTestMarkerContext flowNoMatch = {&order, 1401};
    FlowTestMarkerContext flowDefault = {&order, 1402};
    FlowTestMarkerContext flowFinal = {&order, 1499};

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestFail500Adapter()).input(&input);
    chain.onFailure(&FlowTestMarker::onFlowFailureHandled, &flowDefault);
    chain.onFailure(&FlowTestMarker::is500, &FlowTestMarker::onStepFailureUnhandled, &flowNoMatch);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    assert(chain.run());
    assert(order.size() == 2);
    assert(order[0] == 1402);
    assert(order[1] == 1499);
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text("Hello Flow").testId("MainText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    loka::dsl::SnapRecord capture;

    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::FindNodeById<TextNode>("MainText"))
              .input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::CaptureNode<TextNode>("SceneFlow", "capture-text", 1, 1))
              .onSuccess(&capture);

    assert(chain.run());
    std::string capturedText;
    assert(capture.get("node", capturedText));
    assert(capturedText == "MainText");
    assert(capture.get("text.value", capturedText));
    assert(capturedText == "Hello Flow");

    scene.unmount();
  }

  {
    const int input = 0;
    loka::dsl::SnapRecord captured;

    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1,
                          loka::dsl::SnapV1("SceneFlow", "timing-assert-ok", "TimingNode", 4, 1)
                              .timingFlushMs(4))
              .input(&input)
        | loka::dsl::Step(2, loka::dsl::testing::AssertSnapIntLessEqual("timing.flush_ms", 5))
              .onSuccess(&captured);

    assert(okChain.run());
    long flushMs = 0;
    assert(captured.getInt("timing.flush_ms", flushMs));
    assert(flushMs == 4);

    FlowErrorCapture failCapture = {0, 0, 0};
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> failChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1,
                          loka::dsl::SnapV1("SceneFlow", "timing-assert-fail", "TimingNode", 5, 1)
                              .timingFlushMs(7))
              .input(&input)
        | loka::dsl::Step(2, loka::dsl::testing::AssertSnapIntLessEqual("timing.flush_ms", 5))
              .onFailure(&FlowTestMarker::captureFailure, &failCapture);

    assert(failChain.run());
    assert(failCapture.calls == 1);
    assert(failCapture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT);
    assert(failCapture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED);
  }

  {
    const int input = 0;

    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1,
                          loka::dsl::SnapV1("SceneFlow", "timing-check-ok", "TimingNode", 6, 1)
                              .timingFlushMs(4))
              .input(&input)
        | loka::dsl::Step(2, loka::dsl::testing::CheckTimingLessEqual("timing.flush_ms", 5));

    assert(okChain.run());

    FlowErrorCapture failCapture = {0, 0, 0};
    loka::dsl::FlowChain<int, loka::dsl::SnapRecord> failChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1,
                          loka::dsl::SnapV1("SceneFlow", "timing-check-fail", "TimingNode", 7, 1)
                              .timingFlushMs(8))
              .input(&input)
        | loka::dsl::Step(2, loka::dsl::testing::CheckTimingLessEqual("timing.flush_ms", 5))
              .onFailure(&FlowTestMarker::captureFailure, &failCapture);

    assert(failChain.run());
    assert(failCapture.calls == 1);
    assert(failCapture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT);
    assert(failCapture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED);
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text("Hello Flow").testId("MainText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    loka::dsl::SnapRecord captured;

    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::FindNodeById<TextNode>("MainText"))
              .input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::CaptureNode<TextNode>("SceneFlow", "capture-assert", 2, 1))
        | loka::dsl::Step(3, loka::dsl::testing::AssertSnapStringEquals("text.value", "Hello Flow"))
              .onSuccess(&captured);

    assert(okChain.run());
    std::string value;
    assert(captured.get("text.value", value));
    assert(value == "Hello Flow");

    FlowErrorCapture failCapture = {0, 0, 0};
    loka::dsl::FlowChain<Scene *, loka::dsl::SnapRecord> failChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::FindNodeById<TextNode>("MainText"))
              .input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::CaptureNode<TextNode>("SceneFlow", "capture-assert-fail", 3, 1))
        | loka::dsl::Step(3, loka::dsl::testing::AssertSnapStringEquals("text.value", "Mismatch"))
              .onFailure(&FlowTestMarker::captureFailure, &failCapture);

    assert(failChain.run());
    assert(failCapture.calls == 1);
    assert(failCapture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT);
    assert(failCapture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text("Ready").testId("StatusText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    Scene *sameScene = 0;

    loka::dsl::FlowChain<Scene *, Scene *> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::CheckText("StatusText", "Ready"))
              .input(&scenePtr)
              .onSuccess(&sameScene);

    assert(okChain.run());
    assert(sameScene == scenePtr);

    FlowErrorCapture failCapture = {0, 0, 0};
    loka::dsl::FlowChain<Scene *, Scene *> failChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::CheckText("StatusText", "Busy"))
              .input(&scenePtr)
              .onFailure(&FlowTestMarker::captureFailure, &failCapture);

    assert(failChain.run());
    assert(failCapture.calls == 1);
    assert(failCapture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT);
    assert(failCapture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text("Ready").testId("StatusText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;

    loka::dsl::FlowChain<Scene *, TextNode *> okChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::FindNodeById<TextNode>("StatusText"))
              .input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::AssertTextEquals("Ready"));

    assert(okChain.run());

    FlowErrorCapture capture = {0, 0, 0};
    loka::dsl::FlowChain<Scene *, TextNode *> failChain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::FindNodeById<TextNode>("StatusText"))
              .input(&scenePtr)
        | loka::dsl::Step(2, loka::dsl::testing::AssertTextEquals("Busy"))
              .onFailure(&FlowTestMarker::captureFailure, &capture);

    assert(failChain.run());
    assert(capture.calls == 1);
    assert(capture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT);
    assert(capture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Button("Press").testId("ActionButton");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<Scene *, TextNode *> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::FindNodeById<TextNode>("ActionButton"))
              .input(&scenePtr)
              .onFailure(&FlowTestMarker::captureFailure, &capture);

    assert(chain.run());
    assert(capture.calls == 1);
    assert(capture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_SCENARIO);
    assert(capture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_NODE_TYPE_MISMATCH);

    scene.unmount();
  }

  {
    using namespace loka::app;
    using namespace loka::app::scene;

    NodeComposition composition;
    BoxDefinition &root = composition.declare(Box().testId("RootBox"));
    root << Text("A").testId("DupText");
    root << Text("B").testId("DupText");

    Scene scene(composition.root()->clone());
    FlowScenePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);

    Scene *scenePtr = &scene;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<Scene *, TextNode *> chain =
        loka::dsl::Flow()
        | loka::dsl::Step(1, loka::dsl::testing::FindNodeById<TextNode>("DupText"))
              .input(&scenePtr)
              .onFailure(&FlowTestMarker::captureFailure, &capture);

    assert(chain.run());
    assert(capture.calls == 1);
    assert(capture.kind == loka::dsl::testing::FLOW_ERROR_KIND_SCENE_SCENARIO);
    assert(capture.code == loka::dsl::testing::FLOW_ERROR_SCENE_TEST_DUPLICATE_TEST_ID);

    scene.unmount();
  }

  {
    loka::app::FileChooserResult fileResult;
    fileResult.kind = loka::app::FileChooserResult::RESULT_FILE;
    fileResult.item = loka::file::File::FromPath(loka::core::String::Literal("C:/tmp/a.png"));
    simpleviewer::ChooserContext context;
    simpleviewer::ChooserProjection projection;

    loka::dsl::FlowChain<loka::app::FileChooserResult, simpleviewer::ChooserProjection> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, simpleviewer::ChooserToContextAdapter())
                .input(&fileResult)
                .onSuccess(&context)
          | loka::dsl::Step(2, simpleviewer::ContextToProjectionAdapter())
                .onSuccess(&projection);

    assert(chain.run());
    assert(projection.request.source == loka::core::resource::BLOB_SOURCE_FILE);
    assert(!projection.request.filePath.empty());
    assert(projection.request.tag.equals(loka::core::String::Literal("image-file")));
    assert(projection.message.equals(loka::core::String::Literal("Loka file: C:/tmp/a.png")));
  }

  {
    loka::app::FileChooserResult canceled;
    canceled.kind = loka::app::FileChooserResult::RESULT_CANCELED;
    simpleviewer::ChooserContext context;
    simpleviewer::ChooserProjection projection;

    loka::dsl::FlowChain<loka::app::FileChooserResult, simpleviewer::ChooserProjection> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, simpleviewer::ChooserToContextAdapter())
                .input(&canceled)
                .onSuccess(&context)
          | loka::dsl::Step(2, simpleviewer::ContextToProjectionAdapter())
                .onSuccess(&projection);

    assert(chain.run());
    assert(projection.request.source == loka::core::resource::BLOB_SOURCE_NONE);
    assert(projection.message.equals(loka::core::String::Literal("Canceled")));
  }

  {
    loka::app::FileChooserResult folder = loka::app::FileChooserResult::Folder(
        loka::file::File::FromPath(loka::core::String::Literal("C:/tmp/images")));
    simpleviewer::ChooserContext context;
    simpleviewer::ChooserProjection projection;

    loka::dsl::FlowChain<loka::app::FileChooserResult, simpleviewer::ChooserProjection> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, simpleviewer::ChooserToContextAdapter())
                .input(&folder)
                .onSuccess(&context)
          | loka::dsl::Step(2, simpleviewer::ContextToProjectionAdapter())
                .onSuccess(&projection);

    assert(chain.run());
    assert(projection.request.source == loka::core::resource::BLOB_SOURCE_NONE);
    assert(projection.request.filePath.empty());
    assert(projection.message.equals(loka::core::String::Literal("Loka folder: C:/tmp/images")));
  }

  {
    loka::app::FileChooserResult errorResult = loka::app::FileChooserResult::Error(42);
    simpleviewer::ChooserContext context;
    simpleviewer::ChooserProjection projection;

    loka::dsl::FlowChain<loka::app::FileChooserResult, simpleviewer::ChooserProjection> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, simpleviewer::ChooserToContextAdapter())
                .input(&errorResult)
                .onSuccess(&context)
          | loka::dsl::Step(2, simpleviewer::ContextToProjectionAdapter())
                .onSuccess(&projection);

    assert(chain.run());
    assert(projection.request.source == loka::core::resource::BLOB_SOURCE_NONE);
    assert(projection.request.filePath.empty());
    assert(projection.message.equals(loka::core::String::Literal("Error 42")));
  }

  {
    loka::app::FileChooserResult fileEmptyPath;
    fileEmptyPath.kind = loka::app::FileChooserResult::RESULT_FILE;
    simpleviewer::ChooserContext context;
    simpleviewer::ChooserProjection projection;

    loka::dsl::FlowChain<loka::app::FileChooserResult, simpleviewer::ChooserProjection> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, simpleviewer::ChooserToContextAdapter())
                .input(&fileEmptyPath)
                .onSuccess(&context)
          | loka::dsl::Step(2, simpleviewer::ContextToProjectionAdapter())
                .onSuccess(&projection);

    assert(chain.run());
    assert(projection.request.source == loka::core::resource::BLOB_SOURCE_NONE);
    assert(projection.request.filePath.empty());
    assert(projection.request.tag.empty());
    assert(projection.message.equals(loka::core::String::Literal("Loka file: (unknown)")));
  }

  {
    loka::core::resource::Blob blob = loka::core::resource::Blob::Create();
    simpleviewer::BlobDecodeAttempt attempt;
    loka::core::resource::Image image;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<loka::core::resource::Blob, loka::core::resource::Image> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, simpleviewer::BlobToDecodeAttemptAdapter(0))
                .input(&blob)
                .onSuccess(&attempt)
                .onFailure(&FlowTestMarker::captureFailure, &capture)
          | loka::dsl::Step(2, simpleviewer::DecodeAttemptToImageAdapter())
                .onSuccess(&image);

    assert(chain.run());
    assert(!image.isValid());
    assert(capture.calls == 1);
    assert(capture.kind == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_DECODE);
    assert(capture.code == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_CODE_PLATFORM_CONTEXT_MISSING);
  }

  {
    FlowTestPlatformContext ctx;
    ctx.createImageResult_ = true;
    loka::core::resource::Blob blob = loka::core::resource::Blob::Create();
    simpleviewer::BlobDecodeAttempt attempt;
    loka::core::resource::Image image;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<loka::core::resource::Blob, loka::core::resource::Image> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, simpleviewer::BlobToDecodeAttemptAdapter(&ctx))
                .input(&blob)
                .onSuccess(&attempt)
                .onFailure(&FlowTestMarker::captureFailure, &capture)
          | loka::dsl::Step(2, simpleviewer::DecodeAttemptToImageAdapter())
                .onSuccess(&image);

    assert(chain.run());
    assert(ctx.createImageCalls_ == 1);
    assert(image.isValid());
    assert(image.width() == 16);
    assert(image.height() == 16);
    assert(capture.calls == 0);
  }

  {
    FlowTestPlatformContext ctx;
    ctx.createImageResult_ = false;
    loka::core::resource::Blob blob = loka::core::resource::Blob::Create();
    simpleviewer::BlobDecodeAttempt attempt;
    loka::core::resource::Image image;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<loka::core::resource::Blob, loka::core::resource::Image> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, simpleviewer::BlobToDecodeAttemptAdapter(&ctx))
                .input(&blob)
                .onSuccess(&attempt)
                .onFailure(&FlowTestMarker::captureFailure, &capture)
          | loka::dsl::Step(2, simpleviewer::DecodeAttemptToImageAdapter())
                .onSuccess(&image);

    assert(chain.run());
    assert(ctx.createImageCalls_ == 1);
    assert(!image.isValid());
    assert(capture.calls == 1);
    assert(capture.kind == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_DECODE);
    assert(capture.code == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_CODE_IMAGE_DECODE_FAILED);
  }

  {
    int input = 10;
    int calls = 0;
    bool ready = false;
    int captured = 0;

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestMul2Adapter())
                .input(&input)
          | loka::dsl::Step(2, FlowTestPendingThenSuccessAdapter(&ready, &calls))
                .onSuccess(&captured);

    const loka::dsl::FlowRunResult first = chain.runResult();
    assert(first == loka::dsl::FLOW_RUN_PENDING);
    assert(calls == 1);

    ready = true;
    const loka::dsl::FlowRunResult resumed = chain.resumeResult(2);
    assert(resumed == loka::dsl::FLOW_RUN_SUCCEEDED);
    assert(calls == 2);
    assert(captured == 120);
  }

  {
    loka::core::MutableState<int> trigger;
    int calls = 0;
    bool ready = false;
    int captured = 0;

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestPendingThenSuccessAdapter(&ready, &calls))
                .onSuccess(&captured);
    chain.bindTrigger(&trigger);

    trigger.set(9, true);
    assert(calls == 1);
    assert(captured == 0);

    chain.cancel();
    ready = true;
    const loka::dsl::FlowRunResult canceled = chain.resumeResult(1);
    assert(canceled == loka::dsl::FLOW_RUN_CANCELED);
    assert(calls == 1);
    assert(captured == 0);

    // Cancel terminal must not consume pending step output.
    assert(calls == 1);
    assert(captured == 0);
  }

  {
    int input = 5;
    loka::core::MutableState<int> result;

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestMul2Adapter())
                .input(&input)
                .onSuccess(&result);

    assert(chain.run());
    assert(result.get() == 10);
  }

  {
    int input = 7;
    loka::core::MutableState<int> result;
    loka::core::PushStateTracker tracker;
    tracker.addState(&result);

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestMul2Adapter())
                .input(&input)
                .onSuccess(&result);
    chain.withTracker(&tracker);

    assert(chain.run());
    assert(result.get() == 14);
  }

  {
    int input = 1;
    int callsA = 0;
    std::vector<int> order;
    FlowTestMarkerContext flowFinal = {&order, 1599};

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestCountedPassAdapter(&callsA))
                .input(&input)
                .onSuccess(&input, 1);
    chain.onFinally(&FlowTestMarker::onStepFinally, &flowFinal);

    const loka::dsl::FlowRunResult result = chain.runResult();
    assert(result == loka::dsl::FLOW_RUN_FAILED);
    assert(callsA <= 1024);
    assert(order.size() == 1);
    assert(order[0] == 1599);
  }

  // --- ProjectionToBlobAdapter: source=NONE (cancel) → empty Blob, success ---
  {
    simpleviewer::ChooserProjection projection;
    // default: source == BLOB_SOURCE_NONE
    loka::core::resource::Blob blob;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<simpleviewer::ChooserProjection, loka::core::resource::Blob> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, simpleviewer::ProjectionToBlobAdapter())
                .input(&projection)
                .onFailure(&FlowTestMarker::captureFailure, &capture)
                .onSuccess(&blob);

    assert(chain.run());
    assert(capture.calls == 1);
    assert(capture.kind == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_BLOB_LOAD);
    assert(capture.code == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_CODE_NO_FILE_SELECTED);
  }

  // --- ProjectionToBlobAdapter: source=FILE → reads bytes from file ---
  {
    const char *tmpPath = "_loka_test_blob_adapter.bin";
    {
      const unsigned char data[] = {0xDE, 0xAD, 0xBE, 0xEF};
      FILE *out = std::fopen(tmpPath, "wb");
      assert(out != 0);
      const std::size_t written = std::fwrite(data, 1, sizeof(data), out);
      assert(written == sizeof(data));
      assert(std::fclose(out) == 0);
    }

    simpleviewer::ChooserProjection projection;
    projection.request.setFilePath(loka::core::String::Literal(tmpPath));

    loka::core::resource::Blob blob;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<simpleviewer::ChooserProjection, loka::core::resource::Blob> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, simpleviewer::ProjectionToBlobAdapter())
                .input(&projection)
                .onSuccess(&blob)
                .onFailure(&FlowTestMarker::captureFailure, &capture);

    assert(chain.run());
    assert(capture.calls == 0);
    assert(blob.isValid());
    assert(blob.bytes().size() == 4);
    assert(blob.bytes()[0] == 0xDE);
    assert(blob.bytes()[1] == 0xAD);
    assert(blob.bytes()[2] == 0xBE);
    assert(blob.bytes()[3] == 0xEF);
    assert(blob.isCompleted());

    std::remove(tmpPath);
  }

  // --- ProjectionToBlobAdapter: source=FILE, missing file → failure ---
  {
    simpleviewer::ChooserProjection projection;
    projection.request.setFilePath(loka::core::String::Literal("_loka_nonexistent_file.bin"));

    loka::core::resource::Blob blob;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<simpleviewer::ChooserProjection, loka::core::resource::Blob> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, simpleviewer::ProjectionToBlobAdapter())
                .input(&projection)
                .onFailure(&FlowTestMarker::captureFailure, &capture)
                .onSuccess(&blob);

    assert(chain.run());
    assert(capture.calls == 1);
    assert(capture.kind == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_BLOB_LOAD);
    assert(capture.code == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_CODE_STDIO_OPEN_FAILED);
  }

  // --- Full 6-step integrated chain: FileChooserResult → Image ---
  {
    const char *tmpPath = "_loka_test_full_chain.bin";
    {
      const unsigned char data[] = {0x89, 0x50, 0x4E, 0x47}; // fake PNG header
      FILE *out = std::fopen(tmpPath, "wb");
      assert(out != 0);
      const std::size_t written = std::fwrite(data, 1, sizeof(data), out);
      assert(written == sizeof(data));
      assert(std::fclose(out) == 0);
    }

    FlowTestPlatformContext ctx;
    ctx.createImageResult_ = true;

    loka::app::FileChooserResult fileResult;
    fileResult.kind = loka::app::FileChooserResult::RESULT_FILE;
    fileResult.item = loka::file::File::FromPath(loka::core::String::Literal(tmpPath));

    loka::core::resource::Image image;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<loka::app::FileChooserResult, loka::core::resource::Image> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, simpleviewer::ChooserToContextAdapter())
                .input(&fileResult)
          | loka::dsl::Step(2, simpleviewer::ContextToProjectionAdapter())
          | loka::dsl::Step(3, simpleviewer::ProjectionToBlobAdapter())
          | loka::dsl::Step(4, simpleviewer::BlobToDecodeAttemptAdapter(&ctx))
                .onFailure(&FlowTestMarker::captureFailure, &capture)
          | loka::dsl::Step(5, simpleviewer::DecodeAttemptToImageAdapter())
                .onSuccess(&image);

    assert(chain.run());
    assert(capture.calls == 0);
    assert(ctx.createImageCalls_ == 1);
    assert(image.isValid());
    assert(image.width() == 16);
    assert(image.height() == 16);

    std::remove(tmpPath);
  }

  // --- Full 6-step chain: canceled → no-file-selected at blob projection ---
  {
    FlowTestPlatformContext ctx;
    ctx.createImageResult_ = false;

    loka::app::FileChooserResult canceled;
    canceled.kind = loka::app::FileChooserResult::RESULT_CANCELED;

    loka::core::resource::Image image;
    FlowErrorCapture capture = {0, 0, 0};

    loka::dsl::FlowChain<loka::app::FileChooserResult, loka::core::resource::Image> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, simpleviewer::ChooserToContextAdapter())
                .input(&canceled)
          | loka::dsl::Step(2, simpleviewer::ContextToProjectionAdapter())
          | loka::dsl::Step(3, simpleviewer::ProjectionToBlobAdapter())
          | loka::dsl::Step(4, simpleviewer::BlobToDecodeAttemptAdapter(&ctx))
                .onFailure(&FlowTestMarker::captureFailure, &capture)
          | loka::dsl::Step(5, simpleviewer::DecodeAttemptToImageAdapter())
                .onSuccess(&image);
    chain.onFailure(&FlowTestMarker::captureFailure, &capture);

    assert(chain.run());
    assert(!image.isValid());
    assert(capture.calls == 1);
    assert(capture.kind == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_BLOB_LOAD);
    assert(capture.code == simpleviewer::SIMPLE_VIEWER_FLOW_ERROR_CODE_NO_FILE_SELECTED);
  }

  // --- onSuccess field extraction: extract single field into MutableState ---
  {
    int input = 5;
    loka::core::MutableState<int> valueState;
    loka::core::MutableState<int> extraState;

    loka::dsl::FlowChain<int, FlowTestFieldOutput> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestFieldAdapter())
                .input(&input)
                .onSuccess(&valueState, &FlowTestFieldOutput::value)
                .onSuccess(&extraState, &FlowTestFieldOutput::extra);

    assert(chain.run());
    assert(valueState.get() == 50);  // 5 * 10
    assert(extraState.get() == 6);   // 5 + 1
  }

  // --- bindTrigger: auto-execute flow on state change ---
  {
    loka::core::MutableState<int> trigger;
    loka::core::MutableState<int> result;

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestMul2Adapter())
                .onSuccess(&result);
    chain.bindTrigger(&trigger);

    trigger.set(7, true);
    assert(result.get() == 14);  // 7 * 2

    trigger.set(3, true);
    assert(result.get() == 6);   // 3 * 2
  }

  // --- bindTrigger: reentry guard prevents double execution ---
  {
    loka::core::MutableState<int> trigger;
    int calls = 0;

    // Callback that re-sets the trigger during step onSuccess processing.
    // This fires while the flow is still running, so reentry guard must block it.
    struct ReentryHelper {
      static void reSetTrigger(const int &, void *user) {
        loka::core::MutableState<int> *t
            = static_cast<loka::core::MutableState<int> *>(user);
        t->set(99);  // value changes → notifyStateChanged → OnTriggerChanged
      }
    };

    loka::dsl::FlowChain<int, int> chain
        = loka::dsl::Flow()
          | loka::dsl::Step(1, FlowTestCountedPassAdapter(&calls))
                .onSuccess(&ReentryHelper::reSetTrigger, &trigger);
    chain.bindTrigger(&trigger);

    trigger.set(10);
    assert(calls == 1);  // reentry blocked: flow ran only once
    assert(trigger.get() == 99);  // but the trigger value was updated
  }

  // --- State<void>: callback may delete emitter safely ---
  {
    StateNotifyDeleteCtx ctx;
    ctx.state = new loka::core::EmitterState();
    ctx.valueState = 0;
    ctx.destroyCalls = 0;
    ctx.siblingCalls = 0;

    ctx.state->bind(&StateNotifyHelpers::destroySelf, &ctx, false);
    ctx.state->bind(&StateNotifyHelpers::sibling, &ctx, false);
    ctx.state->emit();

    assert(ctx.state == 0);
    assert(ctx.destroyCalls == 1);
    assert(ctx.siblingCalls == 0);
  }

  // --- State<void>: self-unbind runs once across multiple emits ---
  {
    StateNotifyDeleteCtx ctx;
    loka::core::EmitterState state;
    ctx.state = &state;
    ctx.valueState = 0;
    ctx.destroyCalls = 0;
    ctx.siblingCalls = 0;

    state.bind(&StateNotifyHelpers::selfUnbind, &ctx, false);
    state.bind(&StateNotifyHelpers::sibling, &ctx, false);

    state.emit();
    state.emit();

    assert(ctx.destroyCalls == 1);
    assert(ctx.siblingCalls == 2);
  }

  // --- State<void>: unbound sibling must not run in same emit ---
  {
    StateNotifyDeleteCtx ctx;
    loka::core::EmitterState state;
    ctx.state = &state;
    ctx.valueState = 0;
    ctx.destroyCalls = 0;
    ctx.siblingCalls = 0;

    state.bind(&StateNotifyHelpers::unbindSibling, &ctx, false);
    state.bind(&StateNotifyHelpers::sibling, &ctx, false);
    state.emit();

    assert(ctx.destroyCalls == 1);
    assert(ctx.siblingCalls == 0);
  }

  // --- State<T>: callback may delete mutable state safely ---
  {
    StateNotifyDeleteCtx ctx;
    ctx.state = 0;
    ctx.valueState = new loka::core::MutableState<int>(0);
    ctx.destroyCalls = 0;
    ctx.siblingCalls = 0;

    ctx.valueState->bind(&StateNotifyHelpers::destroyValueSelf, &ctx, false);
    ctx.valueState->bind(&StateNotifyHelpers::sibling, &ctx, false);
    ctx.valueState->set(1);

    assert(ctx.valueState == 0);
    assert(ctx.destroyCalls == 1);
    assert(ctx.siblingCalls == 0);
  }

  // --- State<T>: unbound sibling must not run in same notify ---
  {
    StateNotifyDeleteCtx ctx;
    loka::core::MutableState<int> valueState(0);
    ctx.state = 0;
    ctx.valueState = &valueState;
    ctx.destroyCalls = 0;
    ctx.siblingCalls = 0;

    valueState.bind(&StateNotifyHelpers::unbindValueSibling, &ctx, false);
    valueState.bind(&StateNotifyHelpers::sibling, &ctx, false);
    valueState.set(1);

    assert(ctx.destroyCalls == 1);
    assert(ctx.siblingCalls == 0);
  }

  printf("==== [testLokaFlowDslV1Core] end ====\n");
}
