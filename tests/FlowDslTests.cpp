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

    static loka::dsl::FlowHandleResult
    onFlowFailureHandled(const loka::dsl::FlowError &, void *user) {
      FlowTestMarkerContext *ctx = static_cast<FlowTestMarkerContext *>(user);
      ctx->order->push_back(ctx->marker);
      return loka::dsl::FLOW_ERROR_HANDLED;
    }

    static loka::dsl::FlowHandleResult
    onStepFailureUnhandled(const loka::dsl::FlowError &, void *user) {
      FlowTestMarkerContext *ctx = static_cast<FlowTestMarkerContext *>(user);
      ctx->order->push_back(ctx->marker);
      return loka::dsl::FLOW_ERROR_UNHANDLED;
    }

    static loka::dsl::FlowHandleResult
    onStepFailureHandled(const loka::dsl::FlowError &, void *user) {
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
    bool run(const int &in, int &out, loka::dsl::FlowError &) const {
      out = in * 2;
      return true;
    }
  };

  struct FlowTestAdd1Adapter {
    typedef int In;
    typedef int Out;
    bool run(const int &in, int &out, loka::dsl::FlowError &) const {
      out = in + 1;
      return true;
    }
  };

  struct FlowTestFail500Adapter {
    typedef int In;
    typedef int Out;
    bool run(const int &, int &out, loka::dsl::FlowError &error) const {
      out = 0;
      error.kind = 1;
      error.code = 500;
      return false;
    }
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

    loka::dsl::FlowChain<int, int> chain =
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
              .onFailure(&FlowTestMarker::is500,
                         &FlowTestMarker::onStepFailureUnhandled,
                         &failMatchUnhandled)
              .onFailure(&FlowTestMarker::onStepFailureHandled,
                         &failDefaultHandled)
              .onFinally(&FlowTestMarker::onStepFinally, &f2)
        | loka::dsl::Step(3, FlowTestAdd1Adapter())
              .onSuccess(&FlowTestMarker::onStepSuccess, &s3);
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

    loka::dsl::FlowChain<int, int> chain =
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

  printf("==== [testLokaFlowDslV1Core] end ====\n");
}
