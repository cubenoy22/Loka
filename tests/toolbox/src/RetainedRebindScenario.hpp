#ifndef LOKA_TOOLBOX_TESTS_RETAINED_REBIND_SCENARIO_HPP
#define LOKA_TOOLBOX_TESTS_RETAINED_REBIND_SCENARIO_HPP

#include "ScenarioDriverSupport.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "ToolboxWindow.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/controls/PopupMenu.hpp"
#include "context/ToolboxCellContext.hpp"
#include "context/ToolboxEditTextContext.hpp"
#include "context/ToolboxPopupMenuContext.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "testing/scene/ScenarioAudit.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace loka
{
  namespace toolbox_tests
  {
    /** The refresh counters live in the TEST_BUILD diagnostic box and only the
        rebind cells read them; the helper stays here rather than in
        ScenarioDriverSupport, which the non-test Scrapbook vehicle compiles. */
    inline void CaptureRefreshCounters(Window *window, dsl::SnapRecord &record)
    {
      if (!window || !window->scene()) return;
      ToolboxScenePlatformController *controller = static_cast<ToolboxScenePlatformController *>(
          dsl::testing::SceneTestAccess::platformController(*window->scene()));
      if (!controller) return;
      const ToolboxSceneDebugStats &stats = controller->debugStatsForTesting();
      record.setInt("refresh.calls", stats.totalRefreshCalls);
      record.setInt("refresh.rows", stats.totalRefreshRowVisits);
    }

    /** Reuse the driver's existing terminal owner, so its later stop cannot
        append a second terminal from an unstarted portable scenario. */
    template <typename Scenario>
    bool PublishRebindVerdict(Window *window, Scenario &scenario, const char *app, long tick, bool succeeded)
    {
      dsl::SnapRecord record;
      dsl::FlowError error;
      dsl::BuildSnapV1RecordAdapter(app,
                                    scenario.name().c_str(),
                                    "retained-rebind",
                                    tick,
                                    1,
                                    succeeded ? dsl::SnapStatusOk() : dsl::SnapStatusError())
          .run(0, record, error);
      record.set("checkpoint", "final");
      record.set("rail", "toolbox");
      CaptureRefreshCounters(window, record);
      scenario_tests::SetContentBounds(record, ContentLocalBounds(QueryCaptureContentBounds(window)));
      return scenario.publishVerdict(record);
    }

    /** Stack-local checkpoint writer; owns no controller or scene lifetime. */
    class RebindCheckpoints
    {
    public:
      RebindCheckpoints(Window *window,
                        ToolboxScenePlatformController &controller,
                        dsl::testing::ScenarioAuditFile &audit,
                        const char *app,
                        const char *scenario)
          : window_(window),
            controller_(controller),
            audit_(audit),
            app_(app),
            scenario_(scenario),
            step_(0)
      {
      }

      bool record(const char *checkpoint)
      {
        ++this->step_;
        dsl::SnapRecord record;
        dsl::FlowError error;
        dsl::BuildSnapV1RecordAdapter(
            this->app_, this->scenario_, "retained-rebind", this->step_, 1, dsl::SnapStatusOk())
            .run(0, record, error);
        record.set("checkpoint", checkpoint);
        record.set("rail", "toolbox");
        CaptureRefreshCounters(this->window_, record);
        scenario_tests::SetContentBounds(record, ContentLocalBounds(QueryCaptureContentBounds(this->window_)));
        return this->audit_.recordStep(dsl::testing::ScenarioStepTerminal(
                   this->step_, checkpoint, this->step_, this->step_, dsl::FLOW_STEP_SUCCEEDED, error))
               && this->audit_.recordVerdict(record);
      }

      template <typename Definition, typename Node> bool apply(const Definition &definition, Node *node, bool changed)
      {
        app::scene::NodeContext *context = node->getContext();
        const int calls = this->controller_.debugStatsForTesting().totalRefreshCalls;
        const int rows = this->controller_.debugStatsForTesting().totalRefreshRowVisits;
        if (!definition.applyPropsToNode(node) || node->getContext() != context)
          return false;
        const ToolboxSceneDebugStats &after = this->controller_.debugStatsForTesting();
        return changed ? after.totalRefreshCalls == calls + 1 && after.totalRefreshRowVisits > rows
                       : after.totalRefreshCalls == calls && after.totalRefreshRowVisits == rows;
      }

    private:
      Window *window_;
      ToolboxScenePlatformController &controller_;
      dsl::testing::ScenarioAuditFile &audit_;
      const char *app_;
      const char *scenario_;
      int step_;
    };

    /** Compare every monochrome pixel after dirty replay with the context's
        direct draw, before allowing any full render to repair ledger sources.
        The saved pixels are an observation of this run, never a golden. */
    template <typename Context>
    bool
    RebindReplayMatches(Window *window, ToolboxScenePlatformController &controller, const Rect &rect, Context &context)
    {
      const int width = rect.right - rect.left;
      const int height = rect.bottom - rect.top;
      WindowPtr native = window->asToolboxWindow()->window();
      if (width <= 0 || height <= 0 || width > 636 || height > 400 || rect.left < native->portRect.left
          || rect.right > native->portRect.right || rect.top < native->portRect.top
          || rect.bottom > native->portRect.bottom)
        return false;
      GrafPtr previous;
      GetPort(&previous);
      SetPort(native);
      const int renders = controller.debugStatsForTesting().totalRenderCalls;
      controller.renderDirty(rect);
      std::vector<unsigned char> pixels;
      pixels.reserve(static_cast<size_t>(width * height));
      for (short y = rect.top; y < rect.bottom; ++y)
        for (short x = rect.left; x < rect.right; ++x)
          pixels.push_back(GetPixel(x, y) ? 1 : 0);
      EraseRect(&rect);
      context.draw();
      bool same = controller.debugStatsForTesting().totalRenderCalls == renders;
      size_t i = 0;
      for (short y = rect.top; y < rect.bottom; ++y)
        for (short x = rect.left; x < rect.right; ++x)
          if (pixels[i++] != (GetPixel(x, y) ? 1 : 0))
            same = false;
      SetPort(previous);
      return same;
    }

    /** Adapts Cell's direct paint signature without registering another hit. */
    class CellRebindPaint
    {
    public:
      explicit CellRebindPaint(ToolboxCellContext &context)
          : context_(context)
      {
      }
      void draw()
      {
        this->context_.draw(0);
      }

    private:
      ToolboxCellContext &context_;
    };

    /** AppConfig-owned sources outlive every retained projection borrowing them. */
    class RetainedButtonRebind
    {
    public:
      RetainedButtonRebind()
          : enabledA_(true),
            enabledB_(true)
      {
      }
      bool run(Window *window, ToolboxScenePlatformController &controller, dsl::testing::ScenarioAuditFile &audit)
      {
        app::ButtonNode *node = 0;
        dsl::FlowError error;
        if (dsl::testing::LookupNodeById<app::ButtonNode>(window->scene(), "SmirkBench.AddFace", node, error)
                != dsl::FLOW_STEP_SUCCEEDED
            || !node || !node->getContext())
          return false;
        RebindCheckpoints checks(window, controller, audit, "SmirkBench", "retained-button-rebind");
        // Keep the real emitter/control tag; only the tested projections change.
        app::ButtonProps props(node->props);
        props.text("Label A").enabled(&this->enabledA_);
        if (!checks.apply(app::ButtonDefinition(props), node, true) || !checks.record("apply-A"))
          return false;
        core::State<core::String> *ownedAddress = node->props.text_;
        props.text("Label B");
        std::string title;
        short hilite = 0;
        if (!checks.apply(app::ButtonDefinition(props), node, true) || node->props.text_ != ownedAddress
            || !controller.queryButtonValueForTesting(props.onClick_, title, hilite) || title != "Label B"
            || !checks.record("literal-B-before-render"))
          return false;
        props.enabled(&this->enabledB_);
        if (!checks.apply(app::ButtonDefinition(props), node, true) || !checks.record("enabled-B"))
          return false;
        {
          core::StateTrackerGuard guard(window->getTracker());
          this->enabledB_.set(false);
        }
        if (!controller.queryButtonValueForTesting(props.onClick_, title, hilite) || hilite != 255
            || !checks.record("B-disables-native"))
          return false;
        {
          core::StateTrackerGuard guard(window->getTracker());
          this->enabledA_.set(false);
          this->enabledA_.set(true);
        }
        if (!controller.queryButtonValueForTesting(props.onClick_, title, hilite) || hilite != 255
            || !checks.record("A-does-not-enable-native"))
          return false;
        {
          core::StateTrackerGuard guard(window->getTracker());
          this->enabledB_.set(true);
        }
        return controller.queryButtonValueForTesting(props.onClick_, title, hilite) && hilite == 0
               && checks.record("B-enables-native") && checks.apply(app::ButtonDefinition(props), node, false)
               && checks.record("unchanged-B");
      }

    private:
      core::MutableState<bool> enabledA_;
      core::MutableState<bool> enabledB_;
    };

    class RetainedEditTextRebind
    {
    public:
      RetainedEditTextRebind()
          : textA_(core::String::Literal("171")),
            textB_(core::String::Literal("182"))
      {
      }
      bool run(Window *window, ToolboxScenePlatformController &controller, dsl::testing::ScenarioAuditFile &audit)
      {
        app::EditTextNode *node = 0;
        dsl::FlowError error;
        if (dsl::testing::ResolveSelector<app::EditTextNode>(
                window->scene(), dsl::testing::Within("HelloWorld.Bmi").descendant<app::EditTextNode>(1), node, error)
                != dsl::FLOW_STEP_SUCCEEDED
            || !node || !node->getContext())
          return false;
        ToolboxEditTextContext *context = static_cast<ToolboxEditTextContext *>(node->getContext());
        RebindCheckpoints checks(window, controller, audit, "HelloWorld", "retained-edittext-rebind");
        app::EditTextProps props(node->props);
        props.text(&this->textA_);
        if (!checks.apply(app::EditTextDefinition(props), node, true) || !checks.record("apply-A"))
          return false;
        props.text(&this->textB_);
        std::string native;
        if (!checks.apply(app::EditTextDefinition(props), node, true)
            || !controller.queryEditTextValueForTesting(context, native) || native != "182"
            || !checks.record("B-in-TE-before-render"))
          return false;
        const int beforeA = controller.debugStatsForTesting().textChangedEditControlCount;
        {
          core::StateTrackerGuard guard(window->getTracker());
          this->textA_.set(core::String::Literal("199"));
        }
        if (controller.debugStatsForTesting().textChangedEditControlCount != beforeA
            || !controller.queryEditTextValueForTesting(context, native) || native != "182"
            || !checks.record("A-does-not-notify-TE"))
          return false;
        {
          core::StateTrackerGuard guard(window->getTracker());
          this->textB_.set(core::String::Literal("183"));
        }
        return controller.debugStatsForTesting().textChangedEditControlCount == beforeA + 1
               && controller.queryEditTextValueForTesting(context, native) && native == "183"
               && checks.record("B-notifies-TE") && checks.apply(app::EditTextDefinition(props), node, false)
               && checks.record("unchanged-B");
      }

    private:
      core::MutableState<core::String> textA_;
      core::MutableState<core::String> textB_;
    };

    class RetainedPopupRebind
    {
    public:
      RetainedPopupRebind()
          : selectedA_(0),
            selectedB_(1),
            enabledA_(true),
            enabledB_(true)
      {
        this->itemsA_.push_back(core::String::Literal("Apple A"));
        this->itemsA_.push_back(core::String::Literal("Pear A"));
        this->itemsB_.push_back(core::String::Literal("Apple B"));
        this->itemsB_.push_back(core::String::Literal("Pear B"));
      }
      bool run(Window *window, ToolboxScenePlatformController &controller, dsl::testing::ScenarioAuditFile &audit)
      {
        app::scene::Node *found = 0;
        dsl::FlowError error;
        if (dsl::testing::LookupNodeById<app::scene::Node>(
                window->scene(), "HelloWorld.RightPanel.FruitPopup", found, error)
                != dsl::FLOW_STEP_SUCCEEDED
            || !found || !found->asPopupMenuNode() || !found->getContext())
          return false;
        app::PopupMenuNode *node = found->asPopupMenuNode();
        ToolboxPopupMenuContext *context = static_cast<ToolboxPopupMenuContext *>(node->getContext());
        RebindCheckpoints checks(window, controller, audit, "HelloWorld", "retained-popup-rebind");
        app::PopupMenuProps props(node->props);
        props.items(&this->itemsA_).selectedIndex(&this->selectedA_).enabled(&this->enabledA_);
        if (!checks.apply(app::PopupMenuDefinition(props), node, true) || !checks.record("apply-A"))
          return false;
        props.items(&this->itemsB_).selectedIndex(&this->selectedB_).enabled(&this->enabledB_);
        ToolboxHitLedger::PopupHit hit;
        return checks.apply(app::PopupMenuDefinition(props), node, true)
               && controller.queryPopupHitForTesting(context, hit) && hit.items == &this->itemsB_
               && hit.selectedIndex == &this->selectedB_ && hit.enabled == &this->enabledB_
               && RebindReplayMatches(window, controller, hit.rect, *context)
               && checks.record("B-hit-and-replay-before-render")
               && checks.apply(app::PopupMenuDefinition(props), node, false) && checks.record("unchanged-B");
      }

    private:
      Vector<core::String> itemsA_;
      Vector<core::String> itemsB_;
      core::MutableState<int> selectedA_;
      core::MutableState<int> selectedB_;
      core::MutableState<bool> enabledA_;
      core::MutableState<bool> enabledB_;
    };

    /** Callback counters are scoped to the synchronous click, never stored. */
    class RebindClickObservation
    {
    public:
      RebindClickObservation(core::EmitterState &a, core::EmitterState &b)
          : a_(a),
            b_(b),
            oldClicks_(0),
            newClicks_(0)
      {
        this->a_.bind(&RebindClickObservation::OldClick, this, false);
        this->b_.bind(&RebindClickObservation::NewClick, this, false);
      }
      ~RebindClickObservation()
      {
        this->a_.unbind(&RebindClickObservation::OldClick, this);
        this->b_.unbind(&RebindClickObservation::NewClick, this);
      }
      bool reachedNewOnly() const
      {
        return this->oldClicks_ == 0 && this->newClicks_ == 1;
      }

    private:
      static void OldClick(void *data)
      {
        ++static_cast<RebindClickObservation *>(data)->oldClicks_;
      }
      static void NewClick(void *data)
      {
        ++static_cast<RebindClickObservation *>(data)->newClicks_;
      }
      core::EmitterState &a_;
      core::EmitterState &b_;
      int oldClicks_;
      int newClicks_;
    };

    class RetainedCellRebind
    {
    public:
      RetainedCellRebind()
          : textA_(core::String::Literal("A")),
            textB_(core::String::Literal("B"))
      {
      }
      bool run(Window *window, ToolboxScenePlatformController &controller, dsl::testing::ScenarioAuditFile &audit)
      {
        app::CellNode *node = 0;
        dsl::FlowError error;
        if (dsl::testing::LookupNodeById<app::CellNode>(window->scene(), "MineSweeper.Cell.0", node, error)
                != dsl::FLOW_STEP_SUCCEEDED
            || !node || !node->getContext())
          return false;
        ToolboxCellContext *context = static_cast<ToolboxCellContext *>(node->getContext());
        RebindCheckpoints checks(window, controller, audit, "MineSweeper", "retained-cell-rebind");
        app::CellProps props(node->props);
        props.text(&this->textA_).onClick(&this->clickA_);
        if (!checks.apply(app::CellDefinition(props), node, true) || !checks.record("apply-A"))
          return false;
        props.text(&this->textB_).onClick(&this->clickB_);
        ToolboxHitLedger::CellHit hit;
        CellRebindPaint paint(*context);
        if (!checks.apply(app::CellDefinition(props), node, true) || !controller.queryCellHitForTesting(context, hit)
            || hit.text != &this->textB_ || hit.emitter != &this->clickB_
            || !RebindReplayMatches(window, controller, hit.rect, paint)
            || !checks.record("B-hit-and-replay-before-render"))
          return false;
        RebindClickObservation clicks(this->clickA_, this->clickB_);
        Point point;
        point.h = static_cast<short>((hit.rect.left + hit.rect.right) / 2);
        point.v = static_cast<short>((hit.rect.top + hit.rect.bottom) / 2);
        // The controller returns false after a handled Cell click; its bool
        // requests edit focus, not event delivery. Observe the emitter instead.
        (void)controller.handleMouseDown(point);
        if (!clicks.reachedNewOnly() || !checks.record("click-reaches-B-only"))
          return false;
        return checks.apply(app::CellDefinition(props), node, false) && checks.record("unchanged-B");
      }

    private:
      core::MutableState<core::String> textA_;
      core::MutableState<core::String> textB_;
      core::EmitterState clickA_;
      core::EmitterState clickB_;
    };
  } // namespace toolbox_tests
} // namespace loka
#endif
