#include "ScrapbookScenarios.hpp"

#include "MainNode.hpp"
#include "platform/StringUTF8.hpp"

namespace loka
{
  namespace toolbox_tests
  {
    namespace
    {
      const char *kOpenFirstPage = "open-first-page";
      const char *kOpenFirstPageRefused = "open-first-page-refused";
      const char *kFlipForwardBack = "flip-forward-back";
      const char *kRefusedFlipKeepsPage = "refused-flip-keeps-page";
      const char *kOpenTextPage = "open-text-page";
      const long kStepSpacingTicks = 30;

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
        record.set("test", "LokaTestsToolbox");
        record.set("step", scenario ? scenario : "startup");
        record.set("node", "MainNode");
        record.setInt("tick", tick);
        return record;
      }

      void SetBool(dsl::SnapRecord &record, const char *key, bool value)
      {
        record.set(key, value ? "true" : "false");
      }

      void SetContentBounds(dsl::SnapRecord &record, const ContentBounds &bounds)
      {
        if (!bounds.available)
        {
          record.set("crop_left", "na");
          record.set("crop_top", "na");
          record.set("crop_right", "na");
          record.set("crop_bottom", "na");
          return;
        }
        record.setInt("crop_left", bounds.left);
        record.setInt("crop_top", bounds.top);
        record.setInt("crop_right", bounds.right);
        record.setInt("crop_bottom", bounds.bottom);
      }

      void SetObservedString(dsl::SnapRecord &record, const char *key, const core::String &value, std::string &out)
      {
        out.clear();
        if (!platform::CollectUtf8(value, out))
        {
          record.set(key, "na");
          return;
        }
        record.set(key, out.c_str());
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
    } // namespace

    ScrapbookScenario::ScrapbookScenario(const std::string &name)
        : kind_(KIND_INVALID),
          name_(name),
          stage_(0),
          step1_(),
          step2_()
    {
      if (name == kOpenFirstPage)
      {
        this->kind_ = KIND_OPEN_FIRST_PAGE;
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
    }

    bool
    ScrapbookScenario::step(long tick, scrapbook::MainNode &mainNode, const ContentBounds &bounds, dsl::SnapRecord &out)
    {
      switch (this->kind_)
      {
      case KIND_INVALID:
        out = MakeDriverErrorRecord(this->name_.c_str(), 2302, "scenario is not registered");
        return true;
      case KIND_OPEN_FIRST_PAGE:
      case KIND_OPEN_FIRST_PAGE_REFUSED:
        return this->runOpenScenario(tick, mainNode, bounds, out);
      case KIND_FLIP_FORWARD_BACK:
        return this->runFlipForwardBack(tick, mainNode, bounds, out);
      case KIND_REFUSED_FLIP_KEEPS_PAGE:
        return this->runRefusedFlipKeepsPage(tick, mainNode, bounds, out);
      case KIND_OPEN_TEXT_PAGE:
        return this->runOpenTextPage(tick, mainNode, bounds, out);
      }
      return false;
    }

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
                                            const ContentBounds &bounds,
                                            dsl::SnapRecord &out)
    {
      out = MakeBaseRecord(this->name_.c_str(), tick);
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
      SetObservedString(out, "text", mainNode.displayedPageText(), text);
      const bool refusalReached = text == "Package refused.";
      SetBool(out, "refusal_reached", refusalReached);
      SetContentBounds(out, bounds);

      bool ok = false;
      if (this->kind_ == KIND_OPEN_FIRST_PAGE)
      {
        ok = bounds.available && page.published && page.page == 0 && !refusalReached && page.caption == "1 / 5";
      }
      else
      {
        ok = bounds.available && !page.published && refusalReached && page.caption == "-- / 5";
      }
      SetVerdict(out, ok);
      return true;
    }

    bool ScrapbookScenario::runFlipForwardBack(long tick,
                                               scrapbook::MainNode &mainNode,
                                               const ContentBounds &bounds,
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
      out = MakeBaseRecord(this->name_.c_str(), tick);
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
                                                    const ContentBounds &bounds,
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
      out = MakeBaseRecord(this->name_.c_str(), tick);
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
                                            const ContentBounds &bounds,
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
      out = MakeBaseRecord(this->name_.c_str(), tick);
      setPageObservation(out, "step1_page", "step1_caption", this->step1_);
      setPageObservation(out, "text_page", "text_caption", textPage);
      // The text itself holds a newline, which a snap value cannot carry;
      // record the comparison verdict and the observed length instead.
      SetBool(out, "text_matches_package_asset", textMatches);
      out.setInt("text_length", textAvailable ? static_cast<long>(text.size()) : -1);
      SetContentBounds(out, bounds);
      const bool ok = bounds.available && this->step1_.published && this->step1_.page == 0
                      && this->step1_.caption == "1 / 5" && textPage.published && textPage.page == 4
                      && textPage.caption == "5 / 5" && textMatches;
      SetVerdict(out, ok);
      return true;
    }

    bool IsRegisteredScenario(const std::string &name)
    {
      return name == kOpenFirstPage || name == kOpenFirstPageRefused || name == kFlipForwardBack
             || name == kRefusedFlipKeepsPage || name == kOpenTextPage;
    }

    dsl::SnapRecord MakeDriverErrorRecord(const char *scenario, long errorCode, const char *message)
    {
      dsl::SnapRecord record = MakeBaseRecord(scenario, 0);
      record.set("node", "ScenarioDriver");
      record.set("status", dsl::SnapStatusError());
      record.setInt("error_code", errorCode);
      record.set("error_msg", message ? message : "scenario driver error");
      return record;
    }
  } // namespace toolbox_tests
} // namespace loka
