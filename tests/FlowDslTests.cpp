#include "FlowDslTests.hpp"

#include <cassert>
#include <vector>

#include "loka/dsl/dsl.hpp"

namespace {
  struct FlowTestMarkerContext {
    std::vector<int> *order;
    int marker;
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
    assert(order.size() == 9);
    assert(order[0] == 10);
    assert(order[1] == 11);
    assert(order[2] == 12);
    assert(order[3] == 22);
    assert(order[4] == 20);
    assert(order[5] == 21);
    assert(order[6] == 12);
    assert(order[7] == 22);
    assert(order[8] == 99);
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
    assert(order.size() == 7);
    assert(order[0] == 110);
    assert(order[1] == 111);
    assert(order[2] == 112);
    assert(order[3] == 130);
    assert(order[4] == 132);
    assert(order[5] == 150);
    assert(order[6] == 199);

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

  printf("==== [testLokaFlowDslV1Core] end ====\n");
}
