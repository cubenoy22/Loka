#ifndef LOKA_TESTS_SMIRK_BENCH_ATTRIBUTED_SCENARIO_HPP
#define LOKA_TESTS_SMIRK_BENCH_ATTRIBUTED_SCENARIO_HPP

#include "../../example/SmirkBench/src/MainNode.hpp"
#include "SceneScenarioDriver.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace loka
{
  namespace scenario_tests
  {
    inline dsl::SnapRecord SmirkBenchEditorRecord(const char *scenario, long tick, bool succeeded)
    {
      dsl::SnapRecord record;
      dsl::FlowError error;
      dsl::BuildSnapV1RecordAdapter("SmirkBench",
                                    scenario,
                                    "SmirkBench.EditorLine",
                                    tick,
                                    1,
                                    succeeded ? dsl::SnapStatusOk() : dsl::SnapStatusError())
          .run(0, record, error);
      return record;
    }

    /** Shared checkpoints; the rail owns settling, capture and terminal emission.
        No Scene, node or context pointer survives a checkpoint. */
    inline ScenarioAdvance AdvanceSmirkBenchEditor(long tick,
                                                   app::scene::Scene *scene,
                                                   dsl::testing::ScenarioAuditSink &audit,
                                                   dsl::SnapRecord &out)
    {
      if (tick < 2 || tick == 4)
        return SCENARIO_ADVANCE_PENDING;
      app::scene::Node *lineNode = 0;
      app::scene::Node *wrapNode = 0;
      app::scene::Node *boxNode = 0;
      dsl::FlowError error;
      bool ok = scene
                && dsl::testing::LookupNodeById<app::scene::Node>(scene, "SmirkBench.EditorLine", lineNode, error)
                       == dsl::FLOW_STEP_SUCCEEDED
                && dsl::testing::LookupNodeById<app::scene::Node>(scene, "SmirkBench.WrapFixture", wrapNode, error)
                       == dsl::FLOW_STEP_SUCCEEDED
                && lineNode && wrapNode && lineNode->getContext() && wrapNode->getContext();
      app::AttributedTextNode *line = lineNode ? lineNode->asAttributedTextNode() : 0;
      app::AttributedTextNode *wrap = wrapNode ? wrapNode->asAttributedTextNode() : 0;
      ok = ok && line && wrap
           && dsl::testing::LookupNodeById<app::scene::Node>(scene, "SmirkBench.WrapBox", boxNode, error)
                  == dsl::FLOW_STEP_SUCCEEDED;
      app::BoxNode *box = boxNode ? boxNode->asBoxNode() : 0;
      ok = ok && box && box->props.hasFixedSize() && box->props.effectiveWidth() == 28;
      if (ok)
      {
        using namespace app;
        const AttributedString expected =
            tick <= 3 ? Styled("var x = ", Bold) + Styled("1;", Italic)
                      : Styled("var ", Bold) + Styled("x = ", TextStyle()) + Styled("1;", Italic);
        ok =
            line->props.text_ && line->props.text_->get() == expected && wrap->props.text_
            && wrap->props.text_->get() == Styled("a ab", FontSize<12>() + Bold) + Styled("cd", FontSize<24>() + Italic)
            && wrap->props.blockStyle_.wrap_ == TEXT_WRAP_WORD;
      }
      if (ok && tick == 3)
      {
        smirkbench::MainNode *main =
            static_cast<smirkbench::MainNode *>(dsl::testing::SceneTestAccess::rootBoundary(*scene));
        if (!main)
          ok = false;
        else
          main->changeEditorLineForTesting();
      }
      if (ok)
      {
        ok = audit.recordStep(dsl::testing::ScenarioStepTerminal(static_cast<int>(tick),
                                                                 tick == 2   ? "startup"
                                                                 : tick == 3 ? "change-editor-line"
                                                                             : "capture",
                                                                 tick,
                                                                 tick,
                                                                 dsl::FLOW_STEP_SUCCEEDED,
                                                                 error));
      }
      if (ok && tick < 5)
        return SCENARIO_ADVANCE_PENDING;
      out = SmirkBenchEditorRecord("attributed-editor-line", tick, ok);
      if (ok)
      {
        out.set("text.value", "var x = 1;");
        out.setInt("text.segments", 3);
        out.set("wrap.value", "a abcd");
        out.setInt("wrap.width", 28);
      }
      return SCENARIO_ADVANCE_DRIVER_COMPLETION_READY;
    }

    /** Desktop adapter also supplies the startup reference required by #816.
        Toolbox retains its existing counter-bearing startup audit. */
    class SmirkBenchEditorDriver : public ScenarioDriver
    {
    public:
      SmirkBenchEditorDriver(bool startup, dsl::testing::ScenarioAuditSink *audit)
          : startup_(startup),
            audit_(audit),
            terminal_(audit)
      {
      }

      virtual ScenarioAdvance step(long tick, Window *window, const CaptureContentBounds &bounds, dsl::SnapRecord &out)
      {
        (void)bounds;
        app::scene::Scene *scene = window ? window->scene() : 0;
        if (!this->startup_)
          return AdvanceSmirkBenchEditor(tick, scene, *this->audit_, out);
        if (tick < 2)
          return SCENARIO_ADVANCE_PENDING;
        dsl::FlowError error;
        const bool ok =
            scene
            && dsl::testing::SnapText("SmirkBench.FaceCount", "SmirkBench", "startup", 2, 1).run(scene, out, error)
                   == dsl::FLOW_STEP_SUCCEEDED;
        if (!ok)
          out = SmirkBenchEditorRecord("startup", tick, false);
        return SCENARIO_ADVANCE_DRIVER_COMPLETION_READY;
      }

      virtual bool publishVerdict(const dsl::SnapRecord &record)
      {
        std::string status;
        const bool ok = record.get("status", status) && status == dsl::SnapStatusOk();
        return this->terminal_.emit(ok ? dsl::testing::SCENARIO_AUDIT_SUCCEEDED : dsl::testing::SCENARIO_AUDIT_FAILED,
                                    record);
      }

    private:
      const bool startup_;
      dsl::testing::ScenarioAuditSink *audit_;
      dsl::testing::scenario_audit_detail::TerminalEmitter terminal_;
    };
  } // namespace scenario_tests
} // namespace loka
#endif
