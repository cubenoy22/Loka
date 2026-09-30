#ifndef SMIRKYCARD_CARD_SCENARIO_HPP
#define SMIRKYCARD_CARD_SCENARIO_HPP
#ifdef TEST_BUILD
#include "ScriptRuntime.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace smirkycard
{
  class CardScene;
  class JsCardNode;
  class JsFlowDescription;

  /** Card-owned stable execution slot. Roots die before the card's engine ref.
      Detach cancels synchronously; an active tick owns its final audit delivery. */
  class CardScenario
  {
  public:
    CardScenario(JsCardNode &card, JSValueConst context);
    ~CardScenario();
    bool installContext(JSContext *ctx, JSValueConst context, JSValue capability, JSCFunctionData *method);
    JSValue operation(JSContext *ctx, int argc, JSValueConst *argv, int op);
    void attach(CardScene *scene);
    void cancel();
    void finishDetach();
    void tick();

  private:
    typedef loka::app::scene::Scene *ScenePtr;
    typedef loka::dsl::FlowChain<ScenePtr, ScenePtr> Chain;
    /** The RunOnce wrapper owns entry/outcome caching; this adapter owns only its diagnostic. */
    class JsStepAdapter
    {
    public:
      typedef ScenePtr In;
      typedef ScenePtr Out;
      JsStepAdapter(CardScenario &owner, JSValueConst fn)
          : owner_(&owner),
            fn_(fn)
      {
      }
      loka::dsl::StepRunStatus run(const In &in, Out &out, loka::dsl::FlowError &error) const;
      const char *diagnostic() const
      {
        return this->message_.c_str();
      }

    private:
      CardScenario *owner_;
      JSValue fn_;
      mutable std::string message_;
    };
    /** Separate from Settle: a transition blocks even with empty work queues. */
    class GatedSettle
    {
    public:
      typedef ScenePtr In;
      typedef ScenePtr Out;
      explicit GatedSettle(CardScenario &owner)
          : owner_(&owner)
      {
      }
      loka::dsl::StepRunStatus run(const In &in, Out &out, loka::dsl::FlowError &error) const;

    private:
      CardScenario *owner_;
    };
    bool canAdvance() const;
    JSValue accept(JSContext *ctx, JSValueConst description);
    void complete(loka::dsl::FlowRunResult result);
    bool invoke(JSValueConst function, int argc, JSValueConst *argv, JSValue &result, loka::core::String &error);
    double random();
    enum Phase
    {
      Waiting,
      Invoking,
      Idle,
      Driving,
      Completing,
      Done
    };
    JsCardNode &card_;
    ScriptRuntime &runtime_;
    JsEngine &engine_;
    loka::dsl::testing::ScenarioAuditSink &sink_;
    loka::dsl::testing::ScenarioClock &clock_;
    ScenePtr input_;
    JSValue context_, scenario_, description_, previous_;
    Chain *chain_;
    Phase phase_;
    loka::dsl::FlowRunResult outcome_;
    loka::dsl::FlowError operationError_;
    unsigned long randomState_;
    loka::dsl::testing::scenario_audit_detail::TerminalEmitter terminal_;
    CardScenario(const CardScenario &);
    CardScenario &operator=(const CardScenario &);
  };
} // namespace smirkycard
#endif
#endif
