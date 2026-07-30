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

      dsl::SnapRecord MakeBaseRecord(const char *scenario)
      {
        dsl::SnapRecord record;
        record.setInt("format_version", 1);
        record.setInt("schema_version", 1);
        record.setInt("scenario_version", 1);
        record.set("test", "LokaTestsToolbox");
        record.set("step", scenario ? scenario : "startup");
        record.set("node", "MainNode");
        record.setInt("tick", 1);
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

    bool IsRegisteredScenario(const std::string &name)
    {
      return name == kOpenFirstPage || name == kOpenFirstPageRefused;
    }

    dsl::SnapRecord
    RunRegisteredScenario(const std::string &name, const scrapbook::MainNode &mainNode, const ContentBounds &bounds)
    {
      dsl::SnapRecord record = MakeBaseRecord(name.c_str());
      int pageIndex = -1;
      const bool pagePublished = mainNode.queryCurrentPageIndex(pageIndex);
      SetBool(record, "page_published", pagePublished);
      if (pagePublished)
      {
        record.setInt("page_index", pageIndex);
      }
      else
      {
        record.set("page_index", "na");
      }

      std::string caption;
      std::string text;
      SetObservedString(record, "caption", mainNode.displayedCaption(), caption);
      SetObservedString(record, "text", mainNode.displayedPageText(), text);
      const bool refusalReached = text == "Package refused.";
      SetBool(record, "refusal_reached", refusalReached);
      SetContentBounds(record, bounds);

      bool ok = false;
      if (name == kOpenFirstPage)
      {
        ok = bounds.available && pagePublished && pageIndex == 0 && !refusalReached && caption == "1 / 5";
      }
      else if (name == kOpenFirstPageRefused)
      {
        ok = bounds.available && !pagePublished && refusalReached && caption == "-- / 5";
      }
      SetVerdict(record, ok);
      return record;
    }

    dsl::SnapRecord MakeDriverErrorRecord(const char *scenario, long errorCode, const char *message)
    {
      dsl::SnapRecord record = MakeBaseRecord(scenario);
      record.set("node", "ScenarioDriver");
      record.setInt("tick", 0);
      record.set("status", dsl::SnapStatusError());
      record.setInt("error_code", errorCode);
      record.set("error_msg", message ? message : "scenario driver error");
      return record;
    }
  } // namespace toolbox_tests
} // namespace loka
