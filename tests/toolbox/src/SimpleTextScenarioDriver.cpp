#include "SimpleTextScenarioDriver.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include "MyAppConfig.hpp"
#include "ObservedMainDefinition.hpp"
#include "ScenarioDriverSupport.hpp"
#include "ToolboxFileChoice.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "ToolboxWindow.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "core/util/ScopedPtr.hpp"
#include "platform/StringUTF8.hpp"
#include "testing/scene/ScenarioAudit.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
  // Independent oracle for the original six-row fixture; never use the writer's
  // newline constant to predict its output (the CR mutation must fail).
  const char *const kRows[] = {"Loka writes a small note.",
                               "Six rows fit on this page.",
                               "",
                               "Open, edit, and save.",
                               "A shorter note leaves no tail.",
                               "End of the note."};

  std::string Utf8(const loka::core::String &value)
  {
    std::string text;
    (void)loka::platform::CollectUtf8(value, text);
    return text;
  }
} // namespace

/** Test-TU access only. One transaction hides OPEN/SAVE from the native Shows;
    the production completion and file rails still execute synchronously. */
class SimpleTextTestAccess
{
public:
  static simpletext::MainProps props(SimpleTextAppConfig &config)
  {
    return simpletext::MainProps()
        .platformContext(config.getPlatformContext())
        .newEvent(&config.newEvent_)
        .openEvent(&config.openEvent_)
        .saveEvent(&config.saveEvent_)
        .saveAsEvent(&config.saveAsEvent_);
  }

  static void
  complete(simpletext::MainNode &node, simpletext::Operation operation, const loka::app::FileChooserResult &result)
  {
    loka::core::StateTrackerGuard guard(node.tracker());
    node.operation_.set(operation);
    node.completeChooser(operation, result);
  }

  static bool shorten(simpletext::MainNode &node)
  {
    loka::core::StateTrackerGuard guard(node.tracker());
    while (node.lines_.size() > 1)
      if (node.lines_.remove(node.lines_.at(node.lines_.size() - 1).id) != loka::core::EDIT_OK)
        return false;
    return node.lines_.size() == 1;
  }

  static void save(simpletext::MainNode &node)
  {
    loka::core::StateTrackerGuard guard(node.tracker());
    node.saveDocument();
  }

  static bool matches(const simpletext::MainNode &node, unsigned rows, const char *current)
  {
    if (!node.error_.get().empty() || node.operation_.get() != simpletext::NONE || node.lines_.size() != rows)
      return false;
    if (current ? Utf8(node.currentFile_.item.toString()) != current
                : node.currentFile_.kind != loka::app::FileChooserResult::RESULT_NONE)
      return false;
    for (unsigned i = 0; i < rows; ++i)
      if (Utf8(node.lines_.at(i).value) != (current ? kRows[i] : ""))
        return false;
    return true;
  }

  static void capture(const simpletext::MainNode &node, loka::dsl::SnapRecord &record)
  {
    record.setInt("document.rows", node.lines_.size());
    record.set("document.current",
               node.currentFile_.kind == loka::app::FileChooserResult::RESULT_FILE
                   ? Utf8(node.currentFile_.item.toString()).c_str()
                   : "none");
    record.set("document.error", node.error_.get().empty() ? "none" : Utf8(node.error_.get()).c_str());
    record.set("document.caret_row", "none");
    for (unsigned i = 0; i < node.lines_.size(); ++i)
    {
      char key[40];
      std::sprintf(key, "document.row.%u", i);
      record.set(key, Utf8(node.lines_.at(i).value).c_str());
      if (node.cursor_.state()->get().line == node.lines_.at(i).id)
        record.setInt("document.caret_row", i);
    }
  }
};

namespace loka
{
  namespace toolbox_tests
  {
    namespace
    {
      dsl::SnapRecord MakeRecord(const char *scenario, long tick, const char *status)
      {
        dsl::SnapRecord record;
        dsl::FlowError error;
        dsl::BuildSnapV1RecordAdapter("SimpleText", scenario, "SimpleText.Editor", tick, 1, status)
            .run(0, record, error);
        return record;
      }

      /** Deliberate SimpleViewer presentation/lifecycle twin. Shared mechanics
          remain in ObservedMainDefinition and ScenarioDriverSupport. */
      class SimpleTextScenarioAppConfig : public SimpleTextAppConfig
      {
      public:
        SimpleTextScenarioAppConfig(PlatformContext *context, const dsl::SnapTestConfig::Settings &settings)
            : SimpleTextAppConfig(context),
              scenario_(settings.scenario),
              audit_(ResolveScenarioAuditFile(), settings.scenario.c_str()),
              terminal_(&this->audit_),
              borrowedApp_(0),
              borrowedMain_(0),
              tick_(0),
              lingerRemaining_(settings.hasLingerSeconds ? settings.lingerSeconds : 0.0),
              facts_(MakeRecord(settings.scenario.c_str(), 0, dsl::SnapStatusOk()))
        {
        }
        void setApp(App *app)
        {
          this->borrowedApp_ = app;
        }
        virtual void compose(AppComposition &composition)
        {
          scenario_tests::ObservedMainDefinition<simpletext::MainProps, simpletext::MainNode> mainDefinition(
              SimpleTextTestAccess::props(*this), &this->borrowedMain_);
          composition << WindowDef(WindowProps()
                                       .frame(16, 41, 480, 320)
                                       .scene(mainDefinition)
                                       .title("LokaSimpleText")
                                       .visible(true)
                                       .idlePolicy(app::IdlePolicy::everyTick())
                                       .onIdle(&SimpleTextScenarioAppConfig::OnIdle, this));
        }

      private:
        static void OnIdle(Window *window, double elapsedSeconds, void *userData)
        {
          static_cast<SimpleTextScenarioAppConfig *>(userData)->tick(window, elapsedSeconds);
        }
        bool recordStep(const char *name)
        {
          return this->audit_.recordStep(dsl::testing::ScenarioStepTerminal(static_cast<int>(this->tick_),
                                                                            name,
                                                                            this->tick_,
                                                                            this->tick_,
                                                                            dsl::FLOW_STEP_SUCCEEDED,
                                                                            dsl::FlowError()));
        }
        bool choose(const char *name, simpletext::Operation operation, unsigned rows)
        {
          platform::file::FileHandle handle;
          const file::File item = file::File::Application() << file::File(name);
          if (!this->getPlatformContext()->openFile(item, handle) || !handle.hasSpec)
            return false;
          if (operation == simpletext::SAVE)
          {
            // ResolveApplicationItem uses the app's vRefNum/parID. Require
            // fnfErr here so this cell actually discriminates the create path.
            FSSpec absent;
            if (FSMakeFSSpec(handle.spec.vRefNum, handle.spec.parID, handle.spec.name, &absent) != fnfErr)
              return false;
            handle.spec = absent;
          }
          file::File captured;
          if (!ToolboxCaptureChosenFile(handle.spec, captured))
            return false;
          SimpleTextTestAccess::complete(*this->borrowedMain_, operation, app::FileChooserResult::File(captured));
          return SimpleTextTestAccess::matches(*this->borrowedMain_, rows, name);
        }
        bool inspectSaved(unsigned number, unsigned rows)
        {
          std::string expected;
          for (unsigned i = 0; i < rows; ++i)
          {
            if (i)
              expected += '\r';
            expected += kRows[i];
          }
          platform::file::FileHandle handle;
          long bytes = -1, cr = 0, lf = 0;
          FInfo info = {};
          std::string actual;
          bool read = false;
          bool metadata = false;
          if (this->getPlatformContext()->openFile(file::File::Application() << file::File("Saved"), handle)
              && handle.hasSpec)
          {
            metadata = FSpGetFInfo(&handle.spec, &info) == noErr;
            short ref = 0;
            if (FSpOpenDF(&handle.spec, fsRdPerm, &ref) == noErr)
            {
              // Bound a malformed result too; never allocate from unchecked EOF.
              char buffer[8192];
              if (GetEOF(ref, &bytes) == noErr && bytes >= 0 && bytes <= static_cast<long>(sizeof(buffer)))
              {
                long count = bytes;
                const OSErr error = FSRead(ref, &count, buffer);
                read = error == noErr && count == bytes;
                if (read)
                  actual.assign(buffer, count);
              }
              if (FSClose(ref) != noErr)
                read = false;
            }
          }
          for (std::size_t i = 0; i < actual.size(); ++i)
          {
            if (actual[i] == '\r')
              ++cr;
            if (actual[i] == '\n')
              ++lf;
          }
          char key[40];
          std::sprintf(key, "saved.%u.bytes", number);
          this->facts_.setInt(key, bytes);
          std::sprintf(key, "saved.%u.cr", number);
          this->facts_.setInt(key, cr);
          std::sprintf(key, "saved.%u.lf", number);
          this->facts_.setInt(key, lf);
          // Decimal OSType preserves every unexpected value and is safe audit text.
          std::sprintf(key, "saved.%u.type", number);
          this->facts_.setInt(key, static_cast<long>(info.fdType));
          std::sprintf(key, "saved.%u.creator", number);
          this->facts_.setInt(key, static_cast<long>(info.fdCreator));
          const bool match = read && actual == expected;
          std::sprintf(key, "saved.%u.match", number);
          this->facts_.set(key, match ? "yes" : "no");
          return match && metadata && info.fdType == 'TEXT' && info.fdCreator == 'ttxt';
        }
        void finish(Window *window, bool succeeded)
        {
          this->facts_.setInt("tick", this->tick_);
          this->facts_.set("status", succeeded ? dsl::SnapStatusOk() : dsl::SnapStatusError());
          this->facts_.set("rail", "toolbox");
          this->facts_.set("checkpoint", "final");
          if (this->borrowedMain_)
            SimpleTextTestAccess::capture(*this->borrowedMain_, this->facts_);
          if (window)
            scenario_tests::SetContentBounds(this->facts_, ContentLocalBounds(QueryCaptureContentBounds(window)));
          (void)this->terminal_.emit(
              succeeded ? dsl::testing::SCENARIO_AUDIT_SUCCEEDED : dsl::testing::SCENARIO_AUDIT_FAILED, this->facts_);
          (void)this->completionPublisher_.publish(window);
        }
        void tick(Window *window, double elapsedSeconds)
        {
          if (this->terminal_.isSettled())
          {
            this->lingerRemaining_ -= elapsedSeconds;
            if (this->lingerRemaining_ <= 0.0 && this->borrowedApp_)
              this->borrowedApp_->quit();
            return;
          }
          if (!window || !window->scene() || !this->borrowedMain_)
          {
            this->finish(window, false);
            return;
          }
          app::scene::Scene *scene = window->scene();
          ToolboxScenePlatformController *controller =
              static_cast<ToolboxScenePlatformController *>(dsl::testing::SceneTestAccess::platformController(*scene));
          if (!controller)
          {
            this->finish(window, false);
            return;
          }
          if (scene->hasPendingInvalidation() || controller->hasPendingSync()
              || window->asToolboxWindow()->hasPendingInvalidate())
            return;
          ++this->tick_;
          if (this->tick_ < 2)
            return;
          if (this->tick_ >= 60)
          {
            this->finish(window, false);
            return;
          }
          bool ok = true;
          const char *step = "final";
          if (this->scenario_ == "startup")
          {
            this->finish(window, SimpleTextTestAccess::matches(*this->borrowedMain_, 1, 0));
            return;
          }
          if (this->tick_ == 2)
          {
            step = "open-readme";
            ok = this->choose("ReadMe", simpletext::OPEN, 6);
          }
          else if (this->scenario_ == "open-readme")
          {
            this->finish(window, true);
            return;
          }
          else if (this->tick_ == 3)
          {
            step = "save-create";
            ok = this->choose("Saved", simpletext::SAVE, 6);
            // Capture file facts even when the production completion failed.
            ok = this->inspectSaved(1, 6) && ok;
          }
          else if (this->tick_ == 4)
          {
            step = "shorten";
            ok = SimpleTextTestAccess::shorten(*this->borrowedMain_)
                 && SimpleTextTestAccess::matches(*this->borrowedMain_, 1, "Saved");
          }
          else if (this->tick_ == 5)
          {
            step = "save-overwrite";
            SimpleTextTestAccess::save(*this->borrowedMain_);
            ok = this->inspectSaved(2, 1) && SimpleTextTestAccess::matches(*this->borrowedMain_, 1, "Saved");
          }
          else if (this->tick_ == 6)
          {
            step = "open-saved";
            ok = this->choose("Saved", simpletext::OPEN, 1);
          }
          else
          {
            this->finish(window, true);
            return;
          }
          if (!ok || !this->recordStep(step))
            this->finish(window, false);
        }

        const std::string scenario_;
        dsl::testing::ScenarioAuditFile audit_;
        dsl::testing::scenario_audit_detail::TerminalEmitter terminal_;
        App *borrowedApp_;
        simpletext::MainNode *borrowedMain_;
        long tick_;
        double lingerRemaining_;
        dsl::SnapRecord facts_;
        ScenarioCompletionPublisher completionPublisher_;
      };
    } // namespace

    int RunSimpleTextScenarioApplication()
    {
      dsl::SnapTestConfig::Settings settings;
      if (!dsl::SnapTestConfig::load("LokaTest.cfg", settings) || !settings.hasScenario
          || (settings.scenario != "startup" && settings.scenario != "open-readme"
              && settings.scenario != "save-roundtrip"))
      {
        (void)WriteScenarioErrorAudit("startup", MakeRecord("startup", 0, dsl::SnapStatusError()));
        return 0;
      }
      platform::InitPlatformRuntime();
      core::ScopedPtr<PlatformContext> context(platform::CreatePlatformContext());
      assert(context.get() && "PlatformContext is required");
      if (!context.get())
        return 1;
      SimpleTextScenarioAppConfig config(context.get(), settings);
      core::ScopedPtr<App> app(context->createApp(&config, 0, 0));
      assert(app.get() && "App is required");
      if (!app.get())
        return 1;
      config.setApp(app.get());
      app->run();
      return 0;
    }
  } // namespace toolbox_tests
} // namespace loka
