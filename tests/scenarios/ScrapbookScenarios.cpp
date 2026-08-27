#include "ScrapbookScenarios.hpp"

#include <cassert>

#include "../../example/ScrapbookUI/src/MainNode.hpp"
#include "../../example/ScrapbookUI/src/ScrapbookSceneIds.hpp"
#include "platform/StringUTF8.hpp"

namespace loka
{
  namespace scenario_tests
  {
    namespace
    {
      const char *kStartup = "startup";
      const char *kOpenFirstPageRefused = "open-first-page-refused";
      const char *kFlipForwardBack = "flip-forward-back";
      const char *kRefusedFlipKeepsPage = "refused-flip-keeps-page";
      const char *kOpenTextPage = "open-text-page";
      const char *kOpenTextPageRefused = "open-text-page-refused";
      const long kInitialPresentationTick = 2;
      const long kStepSpacingTicks = 30;
#ifdef TEST_BUILD
      const char *kStandaloneTour = "standalone-tour";
      const long kTourPageHoldTicks = 15;
#endif

      // The exact bytes of Assets/page5.txt as lrpc packed them, trailing
      // newline included. Matching against this pins that the on-screen text
      // is the package's string asset, not a literal that happens to agree.
      const char *kPage5Text = "The Scrapbook keeps each page in its own LRPK bag.\n";

      dsl::SnapRecord MakeBaseRecord(const char *scenario, long tick)
      {
        dsl::SnapRecord record;
        record.setInt("format_version", 1);
        record.setInt("schema_version", 1);
        record.setInt("scenario_version", 1);
        record.set("test", "ScrapbookUI");
        record.set("step", scenario ? scenario : "startup");
        record.set("node", "MainNode");
        record.setInt("tick", tick);
        return record;
      }

      void SetBool(dsl::SnapRecord &record, const char *key, bool value)
      {
        record.set(key, value ? "true" : "false");
      }

      void SetVerdict(dsl::SnapRecord &record, bool ok)
      {
        record.set("status", ok ? dsl::SnapStatusOk() : dsl::SnapStatusError());
        if (!ok)
        {
          record.setInt("error_code", 2301);
          record.set("error_msg", "scenario expectations were not met");
        }
      }

      bool IsRigScenario(const std::string &name)
      {
        return name == kStartup || name == kOpenFirstPageRefused
               || name == kFlipForwardBack || name == kRefusedFlipKeepsPage
               || name == kOpenTextPage || name == kOpenTextPageRefused;
      }

#ifdef TEST_BUILD
      dsl::FlowChain<app::scene::Scene *, dsl::SnapRecord>
      BuildStandaloneTourFlow(dsl::testing::ScenarioClock &clock,
                              app::scene::Scene **sceneInput,
                              dsl::SnapRecord *recordOut,
                              dsl::testing::ScenarioAuditSink *audit)
      {
        using namespace dsl::testing;
        const long page1Tick = kTourPageHoldTicks;
        const long page2Tick = page1Tick + kTourPageHoldTicks;
        const long page3Tick = page2Tick + kTourPageHoldTicks;
        const long page4Tick = page3Tick + kTourPageHoldTicks;
        const long finalTick = page4Tick + kTourPageHoldTicks;
        return (ScenarioFlow(clock, sceneInput).auditTo(audit)
                | AtTick(page1Tick, CheckText(scrapbook::scene_ids::PageCaption(), "1 / 5"))
                      .named("verify-page-1")
                | AtTick(page1Tick, ClickButton(scrapbook::scene_ids::NextButton()))
                      .named("advance-to-page-2")
                | AtTick(page2Tick, CheckText(scrapbook::scene_ids::PageCaption(), "2 / 5"))
                      .named("verify-page-2")
                | AtTick(page2Tick, ClickButton(scrapbook::scene_ids::NextButton()))
                      .named("advance-to-page-3")
                | AtTick(page3Tick, CheckText(scrapbook::scene_ids::PageCaption(), "3 / 5"))
                      .named("verify-page-3")
                | AtTick(page3Tick, ClickButton(scrapbook::scene_ids::NextButton()))
                      .named("advance-to-page-4")
                | AtTick(page4Tick, CheckText(scrapbook::scene_ids::PageCaption(), "4 / 5"))
                      .named("verify-page-4")
                | AtTick(page4Tick, ClickButton(scrapbook::scene_ids::NextButton()))
                      .named("advance-to-page-5")
                | AtTick(finalTick, CheckText(scrapbook::scene_ids::PageCaption(), "5 / 5"))
                      .named("verify-page-5")
                | AtTick(finalTick, CheckText(scrapbook::scene_ids::PageText(), kPage5Text))
                      .named("verify-final-text")
                | AtTick(finalTick, CaptureViewTarget(scrapbook::scene_ids::PageText()))
                      .named("select-final-capture")
                | AtTick(finalTick, CaptureView("ScrapbookUI", "standalone-tour", finalTick, 1))
                      .named("capture-final-page")
                      .onSuccess(recordOut))
            .flow();
      }
#endif
    } // namespace

    ScenarioLaunchPlan::ScenarioLaunchPlan()
        : valid_(false),
          scenario_(),
          completionPolicy_(SCENARIO_COMPLETION_DRIVER_OWNED)
    {
    }

    ScenarioLaunchPlan::ScenarioLaunchPlan(const std::string &scenario,
                                           ScenarioCompletionPolicy completionPolicy)
        : valid_(true),
          scenario_(scenario),
          completionPolicy_(completionPolicy)
    {
    }

#ifdef TEST_BUILD
    ScenarioLaunchPlan ScenarioLaunchPlan::StandaloneTour()
    {
      return ScenarioLaunchPlan(kStandaloneTour, SCENARIO_COMPLETION_HOLD_FINAL_SCENE);
    }

    ScrapbookScenario::StandaloneTourState::StandaloneTourState()
        : clock_(),
          scene_(0),
          record_(),
          flow_()
    {
    }

    void ScrapbookScenario::StandaloneTourState::start(dsl::testing::ScenarioAuditSink *audit)
    {
      assert(!this->flow_.isValid() && "StandaloneTourState starts once");
      this->flow_.set(BuildStandaloneTourFlow(this->clock_, &this->scene_, &this->record_, audit));
    }

    dsl::FlowRunResult ScrapbookScenario::StandaloneTourState::run(long tick, app::scene::Scene *scene)
    {
      this->clock_.advanceTo(tick);
      this->scene_ = scene;
      return this->flow_.runResult();
    }

    void ScrapbookScenario::StandaloneTourState::stop()
    {
      this->flow_.cancel();
      const dsl::FlowRunResult result = this->flow_.runResult();
      assert(result == dsl::FLOW_RUN_CANCELED && "Standalone tour cancellation must be terminal");
      (void)result;
    }

    const dsl::SnapRecord &ScrapbookScenario::StandaloneTourState::record() const
    {
      return this->record_;
    }
#endif

    bool ScenarioLaunchPlan::isValid() const
    {
      return this->valid_;
    }

    const std::string &ScenarioLaunchPlan::scenario() const
    {
      return this->scenario_;
    }

    ScenarioCompletionPolicy ScenarioLaunchPlan::completionPolicy() const
    {
      return this->completionPolicy_;
    }

    bool QueryRigLaunchPlan(bool configLoaded,
                            const dsl::SnapTestConfig::Settings &settings,
                            ScenarioLaunchPlan &out)
    {
      if (!configLoaded || !settings.hasScenario || !IsRigScenario(settings.scenario))
      {
        return false;
      }
      const ScenarioLaunchPlan completed(
          settings.scenario, SCENARIO_COMPLETION_DRIVER_OWNED);
      out = completed;
      return true;
    }

    ScrapbookScenario::ScrapbookScenario(const ScenarioLaunchPlan &plan,
                                         dsl::testing::ScenarioAuditSink *audit)
        : plan_(plan),
          kind_(KIND_INVALID),
          terminalState_(SCENARIO_ADVANCE_PENDING),
          stage_(0),
          step1_(),
          step2_(),
          terminalAudit_(audit)
#ifdef TEST_BUILD
          , standaloneTour_()
#endif
    {
      assert(plan.isValid() && "ScenarioLaunchPlan is required");
      const std::string &name = this->plan_.scenario();
      if (name == kStartup)
      {
        this->kind_ = KIND_STARTUP;
      }
      else if (name == kOpenFirstPageRefused)
      {
        this->kind_ = KIND_OPEN_FIRST_PAGE_REFUSED;
      }
      else if (name == kFlipForwardBack)
      {
        this->kind_ = KIND_FLIP_FORWARD_BACK;
      }
      else if (name == kRefusedFlipKeepsPage)
      {
        this->kind_ = KIND_REFUSED_FLIP_KEEPS_PAGE;
      }
      else if (name == kOpenTextPage)
      {
        this->kind_ = KIND_OPEN_TEXT_PAGE;
      }
      else if (name == kOpenTextPageRefused)
      {
        this->kind_ = KIND_OPEN_TEXT_PAGE_REFUSED;
      }
#ifdef TEST_BUILD
      else if (name == kStandaloneTour)
      {
        this->kind_ = KIND_STANDALONE_TOUR;
        this->standaloneTour_.start(audit);
      }
#endif
    }

    ScenarioAdvance
    ScrapbookScenario::step(long tick,
                            app::scene::Scene *scene,
                            scrapbook::MainNode &mainNode,
                            const CaptureContentBounds &bounds,
                            dsl::SnapRecord &out)
    {
#ifndef TEST_BUILD
      (void)scene;
#endif
      if (this->terminalState_ != SCENARIO_ADVANCE_PENDING)
      {
        return this->terminalState_;
      }
      bool complete = false;
      switch (this->kind_)
      {
      case KIND_INVALID:
        out = MakeDriverErrorRecord(this->plan_.scenario().c_str(), 2302, "scenario is not registered");
        complete = true;
        break;
      case KIND_STARTUP:
      case KIND_OPEN_FIRST_PAGE_REFUSED:
        complete = this->runOpenScenario(tick, mainNode, bounds, out);
        break;
      case KIND_FLIP_FORWARD_BACK:
        complete = this->runFlipForwardBack(tick, mainNode, bounds, out);
        break;
      case KIND_REFUSED_FLIP_KEEPS_PAGE:
        complete = this->runRefusedFlipKeepsPage(tick, mainNode, bounds, out);
        break;
      case KIND_OPEN_TEXT_PAGE:
      case KIND_OPEN_TEXT_PAGE_REFUSED:
        complete = this->runOpenTextPage(tick, mainNode, bounds, out);
        break;
#ifdef TEST_BUILD
      case KIND_STANDALONE_TOUR:
        complete = this->runStandaloneTour(tick, scene, mainNode, bounds, out);
        break;
#endif
      }
      if (!complete)
      {
        return SCENARIO_ADVANCE_PENDING;
      }
      switch (this->plan_.completionPolicy())
      {
      case SCENARIO_COMPLETION_DRIVER_OWNED:
        this->terminalState_ = SCENARIO_ADVANCE_DRIVER_COMPLETION_READY;
        break;
      case SCENARIO_COMPLETION_HOLD_FINAL_SCENE:
        if (!this->publishVerdict(out))
        {
          out = MakeDriverErrorRecord(this->plan_.scenario().c_str(), 2306, "scenario audit write failed");
        }
        this->terminalState_ = SCENARIO_ADVANCE_FINAL_SCENE_HELD;
        break;
      }
      return this->terminalState_;
    }

    bool ScrapbookScenario::publishVerdict(const dsl::SnapRecord &record)
    {
      std::string verdict;
      const dsl::testing::ScenarioAuditTerminalStatus status =
          record.get("status", verdict) && verdict == dsl::SnapStatusOk()
              ? dsl::testing::SCENARIO_AUDIT_SUCCEEDED
              : dsl::testing::SCENARIO_AUDIT_FAILED;
      return this->terminalAudit_.emit(status, record);
    }

    const std::string &ScrapbookScenario::name() const
    {
      return this->plan_.scenario();
    }

#ifdef TEST_BUILD
    void ScrapbookScenario::stop()
    {
      if (this->kind_ == KIND_STANDALONE_TOUR && this->terminalState_ == SCENARIO_ADVANCE_PENDING)
      {
        this->standaloneTour_.stop();
        (void)this->terminalAudit_.emit(dsl::testing::SCENARIO_AUDIT_CANCELED);
        this->terminalState_ = SCENARIO_ADVANCE_FINAL_SCENE_HELD;
      }
    }
#endif

    ScrapbookScenario::PageObservation ScrapbookScenario::observePage(const scrapbook::MainNode &mainNode)
    {
      PageObservation observation;
      observation.published = mainNode.queryCurrentPageIndex(observation.page);
      observation.captionAvailable = platform::CollectUtf8(mainNode.displayedCaption(), observation.caption);
      return observation;
    }

    void ScrapbookScenario::setPageObservation(dsl::SnapRecord &record,
                                               const char *pageKey,
                                               const char *captionKey,
                                               const PageObservation &observation)
    {
      if (observation.published)
      {
        record.setInt(pageKey, observation.page);
      }
      else
      {
        record.set(pageKey, "na");
      }
      record.set(captionKey, observation.captionAvailable ? observation.caption.c_str() : "na");
    }

    bool ScrapbookScenario::runOpenScenario(long tick,
                                            const scrapbook::MainNode &mainNode,
                                            const CaptureContentBounds &bounds,
                                            dsl::SnapRecord &out)
    {
      if (tick < kInitialPresentationTick)
      {
        return false;
      }
      out = MakeBaseRecord(this->plan_.scenario().c_str(), tick);
      const PageObservation page = observePage(mainNode);
      SetBool(out, "page_published", page.published);
      if (page.published)
      {
        out.setInt("page_index", page.page);
      }
      else
      {
        out.set("page_index", "na");
      }

      std::string text;
      out.set("caption", page.captionAvailable ? page.caption.c_str() : "na");
      const bool textAvailable = platform::CollectUtf8(mainNode.displayedPageText(), text);
      SetBool(out, "text_available", textAvailable);
      SetBool(out, "text_empty", textAvailable && text.empty());
      if (textAvailable && !text.empty())
      {
        out.set("text", text.c_str());
      }
      const bool refusalReached = textAvailable && text == "Package refused.";
      SetBool(out, "refusal_reached", refusalReached);
      SetContentBounds(out, bounds);

      bool ok = false;
      if (this->kind_ == KIND_STARTUP)
      {
        ok = bounds.available && page.published && page.page == 0 && textAvailable && !refusalReached
             && page.caption == "1 / 5";
      }
      else
      {
        ok = bounds.available && !page.published && textAvailable && refusalReached && page.caption == "-- / 5";
      }
      SetVerdict(out, ok);
      return true;
    }

    bool ScrapbookScenario::runFlipForwardBack(long tick,
                                               scrapbook::MainNode &mainNode,
                                               const CaptureContentBounds &bounds,
                                               dsl::SnapRecord &out)
    {
      if (this->stage_ == 0)
      {
        this->step1_ = observePage(mainNode);
        mainNode.selectPage(1);
        this->stage_ = 1;
        return false;
      }
      if (this->stage_ == 1)
      {
        if (tick < 1 + kStepSpacingTicks)
        {
          return false;
        }
        this->step2_ = observePage(mainNode);
        mainNode.selectPage(0);
        this->stage_ = 2;
        return false;
      }
      if (tick < 1 + 2 * kStepSpacingTicks)
      {
        return false;
      }

      const PageObservation step3 = observePage(mainNode);
      out = MakeBaseRecord(this->plan_.scenario().c_str(), tick);
      setPageObservation(out, "step1_page", "step1_caption", this->step1_);
      setPageObservation(out, "step2_page", "step2_caption", this->step2_);
      setPageObservation(out, "step3_page", "step3_caption", step3);
      SetContentBounds(out, bounds);
      const bool ok = bounds.available && this->step1_.published && this->step1_.page == 0
                      && this->step1_.caption == "1 / 5" && this->step2_.published && this->step2_.page == 1
                      && this->step2_.caption == "2 / 5" && step3.published && step3.page == 0
                      && step3.caption == "1 / 5";
      SetVerdict(out, ok);
      return true;
    }

    bool ScrapbookScenario::runRefusedFlipKeepsPage(long tick,
                                                    scrapbook::MainNode &mainNode,
                                                    const CaptureContentBounds &bounds,
                                                    dsl::SnapRecord &out)
    {
      if (this->stage_ == 0)
      {
        this->step1_ = observePage(mainNode);
        mainNode.selectPage(1);
        this->stage_ = 1;
        return false;
      }
      if (this->stage_ == 1)
      {
        if (tick < 1 + kStepSpacingTicks)
        {
          return false;
        }
        this->step2_ = observePage(mainNode);
        mainNode.selectPage(2);
        this->stage_ = 2;
        return false;
      }
      if (tick < 1 + 2 * kStepSpacingTicks)
      {
        return false;
      }

      const PageObservation keptPage = observePage(mainNode);
      const int selectorPage = mainNode.selectedPage();
      const int refusedPage = mainNode.refusedPage();
      const bool badgeVisible = mainNode.isRefusedBadgeVisible();
      out = MakeBaseRecord(this->plan_.scenario().c_str(), tick);
      setPageObservation(out, "step1_page", "step1_caption", this->step1_);
      setPageObservation(out, "step2_page", "step2_caption", this->step2_);
      setPageObservation(out, "kept_page", "kept_caption", keptPage);
      out.setInt("selector_page", selectorPage);
      out.setInt("refused_page", refusedPage);
      SetBool(out, "badge_visible", badgeVisible);
      SetContentBounds(out, bounds);
      const bool ok = bounds.available && this->step1_.published && this->step1_.page == 0
                      && this->step1_.caption == "1 / 5" && this->step2_.published && this->step2_.page == 1
                      && this->step2_.caption == "2 / 5" && keptPage.published && keptPage.page == 1
                      && keptPage.caption == "2 / 5" && selectorPage == 1 && refusedPage == 2 && badgeVisible;
      SetVerdict(out, ok);
      return true;
    }

    bool ScrapbookScenario::runOpenTextPage(long tick,
                                            scrapbook::MainNode &mainNode,
                                            const CaptureContentBounds &bounds,
                                            dsl::SnapRecord &out)
    {
      if (this->stage_ == 0)
      {
        this->step1_ = observePage(mainNode);
        mainNode.selectPage(4);
        this->stage_ = 1;
        return false;
      }
      if (tick < 1 + kStepSpacingTicks)
      {
        return false;
      }

      const PageObservation textPage = observePage(mainNode);
      std::string text;
      const bool textAvailable = platform::CollectUtf8(mainNode.displayedPageText(), text);
      const bool textMatches = textAvailable && text == kPage5Text;
      const int refusedPage = mainNode.refusedPage();
      const bool badgeVisible = mainNode.isRefusedBadgeVisible();
      out = MakeBaseRecord(this->plan_.scenario().c_str(), tick);
      setPageObservation(out, "step1_page", "step1_caption", this->step1_);
      setPageObservation(out, "text_page", "text_caption", textPage);
      // The text itself holds a newline, which a snap value cannot carry;
      // record the comparison verdict and the observed length instead.
      SetBool(out, "text_matches_package_asset", textMatches);
      out.setInt("text_length", textAvailable ? static_cast<long>(text.size()) : -1);
      out.setInt("refused_page", refusedPage);
      SetBool(out, "badge_visible", badgeVisible);
      SetContentBounds(out, bounds);
      bool ok = false;
      if (this->kind_ == KIND_OPEN_TEXT_PAGE)
      {
        ok = bounds.available && this->step1_.published && this->step1_.page == 0
             && this->step1_.caption == "1 / 5" && textPage.published && textPage.page == 4
             && textPage.caption == "5 / 5" && textMatches;
      }
      else
      {
        // The runner corrupted the page-5 bag on disk, so the flip must be
        // refused and the text must not appear. This is the discrimination
        // that the accepted text really is the package's bytes: a compiled
        // literal would have survived the corruption.
        ok = bounds.available && this->step1_.published && this->step1_.page == 0
             && this->step1_.caption == "1 / 5" && textPage.published && textPage.page == 0
             && textPage.caption == "1 / 5" && refusedPage == 4 && badgeVisible && !textMatches;
      }
      SetVerdict(out, ok);
      return true;
    }

#ifdef TEST_BUILD
    bool ScrapbookScenario::runStandaloneTour(long tick,
                                              app::scene::Scene *scene,
                                              scrapbook::MainNode &mainNode,
                                              const CaptureContentBounds &bounds,
                                              dsl::SnapRecord &out)
    {
      const dsl::FlowRunResult result = this->standaloneTour_.run(tick, scene);
      if (result == dsl::FLOW_RUN_PENDING)
      {
        return false;
      }
      if (result != dsl::FLOW_RUN_SUCCEEDED)
      {
        out = MakeDriverErrorRecord(this->plan_.scenario().c_str(), 2305, "standalone Flow failed");
        return true;
      }
      const PageObservation finalPage = observePage(mainNode);
      std::string text;
      const bool textAvailable = platform::CollectUtf8(mainNode.displayedPageText(), text);
      const bool textMatches = textAvailable && text == kPage5Text;
      out = this->standaloneTour_.record();
      setPageObservation(out, "final_page", "final_caption", finalPage);
      SetBool(out, "text_matches_package_asset", textMatches);
      out.setInt("text_length", textAvailable ? static_cast<long>(text.size()) : -1);
      SetContentBounds(out, bounds);
      const bool ok = bounds.available && finalPage.published && finalPage.page == 4
                      && finalPage.caption == "5 / 5" && textMatches;
      SetVerdict(out, ok);
      return true;
    }
#endif

    dsl::SnapRecord MakeDriverErrorRecord(const char *scenario, long errorCode, const char *message)
    {
      dsl::SnapRecord record = MakeBaseRecord(scenario, 0);
      record.set("node", "ScenarioDriver");
      record.set("status", dsl::SnapStatusError());
      record.setInt("error_code", errorCode);
      record.set("error_msg", message ? message : "scenario driver error");
      return record;
    }
  } // namespace scenario_tests
} // namespace loka
