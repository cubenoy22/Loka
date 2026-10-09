#include "SimpleTextTests.hpp"

// Null/generic-file integration only: desktop runners link their native core
// libraries, so they must not instantiate this POSIX fixture or file rail.
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(LOKA_RETRO68)
#include "../example/SimpleText/src/MyAppConfig.hpp"
#include "app/MenuComposition.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/layout/RowLayout.hpp"
#include "app/layout/ColumnLayout.hpp"
#include <sys/stat.h>
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "support/TestVerify.hpp"
#include "platform/StringUTF8.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

using namespace loka::core;
using namespace loka::app;
using loka::file::File;

class SimpleTextTestAccess
{
public:
  static ObservableList<String> &lines(simpletext::MainNode &n)
  {
    return n.lines_;
  }
  static FileChooserResult current(const simpletext::MainNode &n)
  {
    return n.currentFile_;
  }
  static simpletext::Operation operation(const simpletext::MainNode &n)
  {
    return n.operation_.get();
  }
  static String error(const simpletext::MainNode &n)
  {
    return n.error_.get();
  }
  static LineCursor caret(const simpletext::MainNode &n)
  {
    return n.caret_.get();
  }
  static void postCaret(simpletext::MainNode &n, LineCursor value)
  {
    n.caret_.set(value);
  }
  static void choose(simpletext::MainNode &n, bool save, const FileChooserResult &value)
  {
    (save ? n.saveResult_ : n.openResult_).set(value, true);
  }
  static EmitterState &newEvent(SimpleTextAppConfig &c)
  {
    return c.newEvent_;
  }
  static EmitterState &openEvent(SimpleTextAppConfig &c)
  {
    return c.openEvent_;
  }
  static EmitterState &saveEvent(SimpleTextAppConfig &c)
  {
    return c.saveEvent_;
  }
  static EmitterState &saveAsEvent(SimpleTextAppConfig &c)
  {
    return c.saveAsEvent_;
  }
};

namespace
{
  std::string utf8(const String &value)
  {
    std::string out;
    LOKA_VERIFY(loka::platform::CollectUtf8(value, out));
    return out;
  }
  const MenuItemDefinition *itemAt(const MenuDefinition &menu, unsigned index)
  {
    const MenuItemDefinition *item = menu.itemsHead();
    while (item && index--)
      item = item->nextInComposition;
    return item;
  }
  scene::Node *find(scene::Node *node, const char *id)
  {
    if (!node)
      return 0;
    if (node->testId() == id)
      return node;
    scene::INestable *nest = node->asNestable();
    if (nest)
      for (scene::Node *child = nest->childrenHead(); child; child = child->nextInComposition)
      {
        scene::Node *found = find(child, id);
        if (found)
          return found;
      }
    return 0;
  }
  struct Harness
  {
    std::string directory;
    NullPlatformContext context;
    SimpleTextAppConfig config;
    NullScenePlatformController platform;
    scene::Scene *scene;
    Harness()
        : config(&this->context),
          scene(0)
    {
      char pattern[] = "/tmp/loka-simpletext-XXXXXX";
      char *created = mkdtemp(pattern);
      LOKA_VERIFY(created);
      this->directory = created;
      this->context.setApplicationDirectory(String(this->directory));
      simpletext::MainProps props;
      props.platformContext(&this->context)
          .newEvent(&SimpleTextTestAccess::newEvent(this->config))
          .openEvent(&SimpleTextTestAccess::openEvent(this->config))
          .saveEvent(&SimpleTextTestAccess::saveEvent(this->config))
          .saveAsEvent(&SimpleTextTestAccess::saveAsEvent(this->config));
      this->scene = new scene::Scene(scene::Boundary<simpletext::MainNode>(props).clone());
      this->scene->mount(&this->platform);
      loka::dsl::testing::SceneTestAccess::updateAttached(*this->scene, true);
    }
    ~Harness()
    {
      loka::dsl::testing::SceneTestAccess::unmount(*this->scene);
      delete this->scene;
      std::remove((this->directory + "/first.txt").c_str());
      std::remove((this->directory + "/second.txt").c_str());
      std::remove((this->directory + "/bad.txt").c_str());
      std::remove((this->directory + "/long.txt").c_str());
      LOKA_VERIFY(rmdir(this->directory.c_str()) == 0);
    }
    simpletext::MainNode &main() const
    {
      return *static_cast<simpletext::MainNode *>(loka::dsl::testing::SceneTestAccess::rootNode(*this->scene));
    }
    ObservableList<String> &lines()
    {
      return SimpleTextTestAccess::lines(this->main());
    }
    File file(const char *name)
    {
      return File::Application() << File(name);
    }
    void put(const char *name, const std::string &bytes)
    {
      std::FILE *f = std::fopen((this->directory + "/" + name).c_str(), "wb");
      LOKA_VERIFY(f);
      LOKA_VERIFY(std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size());
      LOKA_VERIFY(std::fclose(f) == 0);
    }
    std::string bytes(const char *name)
    {
      std::FILE *f = std::fopen((this->directory + "/" + name).c_str(), "rb");
      LOKA_VERIFY(f);
      std::string value;
      int ch;
      while ((ch = std::fgetc(f)) != EOF)
        value += static_cast<char>(ch);
      LOKA_VERIFY(!std::ferror(f));
      LOKA_VERIFY(std::fclose(f) == 0);
      return value;
    }
    void flush()
    {
      if (this->scene->hasPendingInvalidation())
        this->scene->flushInvalidation();
    }
    void open(const char *name)
    {
      SimpleTextTestAccess::openEvent(this->config).emit();
      this->flush();
      this->choose(false, FileChooserResult::File(this->file(name)));
    }
    void choose(bool save, const FileChooserResult &result)
    {
      SimpleTextTestAccess::choose(this->main(), save, result);
      this->flush();
    }
    std::string row(unsigned short i)
    {
      return utf8(this->lines().at(i).value);
    }
    bool hasError()
    {
      return !SimpleTextTestAccess::error(this->main()).equals(String());
    }
    void currentIs(const char *name)
    {
      const FileChooserResult current = SimpleTextTestAccess::current(this->main());
      LOKA_VERIFY(current.kind == FileChooserResult::RESULT_FILE);
      LOKA_VERIFY(!(current.item != this->file(name)));
    }
  };
} // namespace

void testSimpleTextOpenSaveAndSaveAs()
{
  Harness h;
  h.put("first.txt", "alpha\nbeta\n");
  h.open("first.txt");
  LOKA_VERIFY(h.lines().size() == 3 && h.row(0) == "alpha" && h.row(1) == "beta" && h.row(2).empty());
  h.currentIs("first.txt");
  LOKA_VERIFY(!h.hasError());
  SimpleTextTestAccess::saveEvent(h.config).emit();
  LOKA_VERIFY(h.bytes("first.txt") == "alpha\nbeta\n");
  LOKA_VERIFY(h.lines().update(h.lines().at(0).id, String("edited")) == EDIT_OK);
  SimpleTextTestAccess::saveAsEvent(h.config).emit();
  h.flush();
  h.choose(true, FileChooserResult::File(h.file("second.txt")));
  h.currentIs("second.txt");
  LOKA_VERIFY(h.bytes("second.txt") == "edited\nbeta\n");
  LOKA_VERIFY(h.bytes("first.txt") == "alpha\nbeta\n");
  LOKA_VERIFY(h.lines().update(h.lines().at(0).id, String("again")) == EDIT_OK);
  SimpleTextTestAccess::saveEvent(h.config).emit();
  LOKA_VERIFY(h.bytes("second.txt") == "again\nbeta\n");
}

void testSimpleTextNewAndSaveWithoutDestination()
{
  Harness h;
  LOKA_VERIFY(h.lines().size() == 1 && h.row(0).empty());
  LOKA_VERIFY(SimpleTextTestAccess::current(h.main()).kind == FileChooserResult::RESULT_NONE);
  SimpleTextTestAccess::saveEvent(h.config).emit();
  h.flush();
  LOKA_VERIFY(SimpleTextTestAccess::operation(h.main()) == simpletext::SAVE);
  LOKA_VERIFY(find(&h.main(), "SimpleText.Save"));
  h.choose(true, FileChooserResult::File(h.file("first.txt")));
  h.currentIs("first.txt");
  LOKA_VERIFY(h.lines().update(h.lines().at(0).id, String("old")) == EDIT_OK);
  h.open("missing.txt");
  LOKA_VERIFY(h.hasError());
  for (unsigned short row = 1; row < TextEditorProps::kMaxLines; ++row)
    LOKA_VERIFY(h.lines().insert(row, String("old")) == EDIT_OK);
  const ItemId old = h.lines().at(0).id;
  SimpleTextTestAccess::newEvent(h.config).emit();
  LOKA_VERIFY(h.lines().size() == 1 && h.row(0).empty() && h.lines().at(0).id != old);
  LOKA_VERIFY(SimpleTextTestAccess::current(h.main()).kind == FileChooserResult::RESULT_NONE);
  LOKA_VERIFY(!h.hasError());
  SimpleTextTestAccess::saveEvent(h.config).emit();
  LOKA_VERIFY(SimpleTextTestAccess::operation(h.main()) == simpletext::SAVE);
  LOKA_VERIFY(h.bytes("first.txt").empty());
}

void testSimpleTextPendingCommandsAndTerminalResults()
{
  Harness h;
  h.put("first.txt", "kept");
  h.open("first.txt");
  LOKA_VERIFY(h.lines().update(h.lines().at(0).id, String("unsaved")) == EDIT_OK);
  for (int saving = 0; saving != 2; ++saving)
  {
    (saving ? SimpleTextTestAccess::saveAsEvent(h.config) : SimpleTextTestAccess::openEvent(h.config)).emit();
    h.flush();
    const ListRevision revision = h.lines().revision().get();
    SimpleTextTestAccess::newEvent(h.config).emit();
    SimpleTextTestAccess::openEvent(h.config).emit();
    SimpleTextTestAccess::saveEvent(h.config).emit();
    SimpleTextTestAccess::saveAsEvent(h.config).emit();
    LOKA_VERIFY(SimpleTextTestAccess::operation(h.main()) == (saving ? simpletext::SAVE : simpletext::OPEN));
    LOKA_VERIFY(!(h.lines().revision().get() != revision));
    LOKA_VERIFY(h.row(0) == "unsaved" && h.bytes("first.txt") == "kept");
    h.currentIs("first.txt");
    h.choose(saving != 0, FileChooserResult());
    LOKA_VERIFY(SimpleTextTestAccess::operation(h.main()) != simpletext::NONE);
    h.choose(saving != 0, FileChooserResult::Canceled());
    LOKA_VERIFY(SimpleTextTestAccess::operation(h.main()) == simpletext::NONE);
    LOKA_VERIFY(!(h.lines().revision().get() != revision));
    LOKA_VERIFY(!h.hasError());
    LOKA_VERIFY(!find(&h.main(), "SimpleText.Open") && !find(&h.main(), "SimpleText.Save"));
  }
  SimpleTextTestAccess::openEvent(h.config).emit();
  h.choose(false, FileChooserResult::Error(17));
  LOKA_VERIFY(SimpleTextTestAccess::operation(h.main()) == simpletext::NONE && h.hasError());
  SimpleTextTestAccess::saveAsEvent(h.config).emit();
  h.choose(true, FileChooserResult::Error(18));
  LOKA_VERIFY(SimpleTextTestAccess::operation(h.main()) == simpletext::NONE && h.hasError());
  LOKA_VERIFY(h.row(0) == "unsaved");
  h.currentIs("first.txt");
}

void testSimpleTextReadAndWriteFailuresPreserveDestination()
{
  Harness h;
  h.put("first.txt", "old");
  h.put("bad.txt", std::string("bad\x80", 4));
  h.open("first.txt");
  const ItemId id = h.lines().at(0).id;
  const ListRevision revision = h.lines().revision().get();
  h.open("bad.txt");
  LOKA_VERIFY(h.hasError() && h.row(0) == "old" && h.lines().at(0).id == id);
  LOKA_VERIFY(utf8(SimpleTextTestAccess::error(h.main())) == "Cannot open the file: it is not plain ASCII text.");
  LOKA_VERIFY(!(h.lines().revision().get() != revision));
  h.currentIs("first.txt");
  h.put("long.txt", std::string(9000, 'a'));
  h.open("long.txt");
  LOKA_VERIFY(utf8(SimpleTextTestAccess::error(h.main())) == "Cannot open the file: it is too long for the editor.");
  h.open("absent.txt");
  LOKA_VERIFY(utf8(SimpleTextTestAccess::error(h.main())) == "Cannot open the file.");
  LOKA_VERIFY(h.row(0) == "old" && h.lines().at(0).id == id);
  h.currentIs("first.txt");
  SimpleTextTestAccess::saveAsEvent(h.config).emit();
  h.choose(true, FileChooserResult::File(h.file("missing/child.txt")));
  LOKA_VERIFY(h.hasError() && h.row(0) == "old");
  LOKA_VERIFY(utf8(SimpleTextTestAccess::error(h.main())).find("destination may have changed") != std::string::npos);
  h.currentIs("first.txt");
  SimpleTextTestAccess::saveAsEvent(h.config).emit();
  h.choose(true, FileChooserResult::File(File("/dev/full")));
  LOKA_VERIFY(h.hasError() && h.row(0) == "old");
  h.currentIs("first.txt");
  LOKA_VERIFY(h.lines().update(id, String("still here")) == EDIT_OK);
  SimpleTextTestAccess::saveEvent(h.config).emit();
  LOKA_VERIFY(h.bytes("first.txt") == "still here" && !h.hasError());
  LOKA_VERIFY(std::remove((h.directory + "/first.txt").c_str()) == 0);
  LOKA_VERIFY(mkdir((h.directory + "/first.txt").c_str(), 0700) == 0);
  SimpleTextTestAccess::saveEvent(h.config).emit();
  LOKA_VERIFY(h.hasError() && h.row(0) == "still here");
  h.currentIs("first.txt");
  LOKA_VERIFY(rmdir((h.directory + "/first.txt").c_str()) == 0);
}

void testSimpleTextCommitExhaustionPreservesDestination()
{
  Harness h;
  h.put("first.txt", "kept");
  h.put("second.txt", "replacement");
  h.open("first.txt");
  // Exhaust the real list identity budget; no production test hook needed.
  while (h.lines().at(0).id.seq != 65535)
  {
    LOKA_VERIFY(h.lines().remove(h.lines().at(0).id) == EDIT_OK);
    LOKA_VERIFY(h.lines().insert(0, String("kept")) == EDIT_OK);
  }
  const ItemId id = h.lines().at(0).id;
  const ListRevision revision = h.lines().revision().get();
  h.open("second.txt");
  LOKA_VERIFY(h.hasError() && h.row(0) == "kept" && h.lines().at(0).id == id);
  LOKA_VERIFY(!(h.lines().revision().get() != revision));
  h.currentIs("first.txt");
  SimpleTextTestAccess::newEvent(h.config).emit();
  LOKA_VERIFY(h.hasError() && h.lines().at(0).id == id);
  h.currentIs("first.txt");
  SimpleTextTestAccess::saveEvent(h.config).emit();
  LOKA_VERIFY(h.bytes("first.txt") == "kept" && h.bytes("second.txt") == "replacement");
}

void testSimpleTextRepeatedOpenAndCaretReplacement()
{
  Harness h;
  h.put("first.txt", "abc\ndef");
  h.open("first.txt");
  const ItemId first = h.lines().at(0).id;
  const ItemId second = h.lines().at(1).id;
  {
    scene::BorrowScope borrow(h.platform);
    // Keep the rail from consuming requests while checking the app's post.
    SimpleTextTestAccess::postCaret(h.main(), LineCursor(second, 2));
    SimpleTextTestAccess::openEvent(h.config).emit();
    SimpleTextTestAccess::choose(h.main(), false, FileChooserResult::File(h.file("first.txt")));
    LOKA_VERIFY(h.lines().at(0).id != first);
    LOKA_VERIFY(SimpleTextTestAccess::caret(h.main()) == LineCursor(h.lines().at(0).id, 0));
  }
  h.flush();
  h.currentIs("first.txt");
  LOKA_VERIFY(SimpleTextTestAccess::operation(h.main()) == simpletext::NONE);
  {
    scene::BorrowScope borrow(h.platform);
    SimpleTextTestAccess::postCaret(h.main(), LineCursor(h.lines().at(1).id, 2));
    SimpleTextTestAccess::saveEvent(h.config).emit();
    LOKA_VERIFY(SimpleTextTestAccess::caret(h.main()) == LineCursor(h.lines().at(1).id, 2));
    SimpleTextTestAccess::newEvent(h.config).emit();
    LOKA_VERIFY(SimpleTextTestAccess::caret(h.main()) == LineCursor(h.lines().at(0).id, 0));
  }
}

void testSimpleTextMenuAndDialogProps()
{
  Harness h;
  const MenuBarDefinition *bar = h.scene->menuBar();
  LOKA_VERIFY(bar && bar->menusCount() == 1);
  const MenuDefinition *file = bar->menuAt(0);
  LOKA_VERIFY(file && file->itemsCount() == 6);
  LOKA_VERIFY(file->title.equals(String("File")));
  const char *titles[] = {"New", "Open...", "Save", "Save As...", "", "Quit"};
  for (int i = 0; i < 6; ++i)
  {
    const MenuItemDefinition *item = itemAt(*file, i);
    LOKA_VERIFY(item && item->title.equals(String(titles[i])));
    LOKA_VERIFY(item->titleState == 0);
    LOKA_VERIFY(item->isSeparator == (i == 4));
    LOKA_VERIFY(item->hasShortcut == (i == 1 || i == 2));
  }
  LOKA_VERIFY(itemAt(*file, 1)->shortcutKey == 'o' && itemAt(*file, 2)->shortcutKey == 's');
  LOKA_VERIFY(itemAt(*file, 5)->action == MENU_ACTION_QUIT_APP);
  LOKA_VERIFY(itemAt(*file, 0)->onClickState == &SimpleTextTestAccess::newEvent(h.config));
  LOKA_VERIFY(itemAt(*file, 1)->onClickState == &SimpleTextTestAccess::openEvent(h.config));
  LOKA_VERIFY(itemAt(*file, 2)->onClickState == &SimpleTextTestAccess::saveEvent(h.config));
  LOKA_VERIFY(itemAt(*file, 3)->onClickState == &SimpleTextTestAccess::saveAsEvent(h.config));
  MenuBarDefinition defaultBar;
  MenuComposition defaultComposition(&defaultBar);
  h.config.composeDefaultMenu(defaultComposition);
  defaultComposition.finish();
  LOKA_VERIFY(defaultBar.menusCount() == 0);
  SimpleTextTestAccess::openEvent(h.config).emit();
  h.flush();
  OpenFileDialogNode *open = static_cast<OpenFileDialogNode *>(find(&h.main(), "SimpleText.Open"));
  LOKA_VERIFY(open && open->props.options_.purpose() == FILE_DIALOG_OPEN);
  LOKA_VERIFY(open->props.options_.filterPolicy() == FILE_DIALOG_FILTER_ALL_FILES_TEXT);
  h.choose(false, FileChooserResult::Canceled());
  SimpleTextTestAccess::saveAsEvent(h.config).emit();
  h.flush();
  OpenFileDialogNode *save = static_cast<OpenFileDialogNode *>(find(&h.main(), "SimpleText.Save"));
  LOKA_VERIFY(save && save->props.options_.purpose() == FILE_DIALOG_SAVE);
  LOKA_VERIFY(save->props.options_.defaultName().equals(String("untitled.txt")));
  LOKA_VERIFY(save->props.options_.filterPolicy() == FILE_DIALOG_FILTER_ALL_FILES_TEXT);
  LOKA_VERIFY(find(&h.main(), "SimpleText.Editor") && find(&h.main(), "SimpleText.Error"));
}

void testSimpleTextRibbonFiresTheMenuEmitters()
{
  using namespace loka::dsl;
  using namespace loka::dsl::testing;
  Harness h;
  LOKA_VERIFY(find(&h.main(), "SimpleText.Ribbon"));
  const char *titles[] = {"New", "Open...", "Save", "Save As..."};
  EmitterState *events[] = {&SimpleTextTestAccess::newEvent(h.config),
                            &SimpleTextTestAccess::openEvent(h.config),
                            &SimpleTextTestAccess::saveEvent(h.config),
                            &SimpleTextTestAccess::saveAsEvent(h.config)};
  FlowError error;
  for (int i = 0; i < 4; ++i)
  {
    ButtonNode *button = 0;
    LOKA_VERIFY(ResolveSelector(h.scene, WithinAnchor("SimpleText.Ribbon").descendant<ButtonNode>(i + 1), button, error)
                == FLOW_STEP_SUCCEEDED);
    LOKA_VERIFY(button && button->props.text_->get().equals(String(titles[i])));
    LOKA_VERIFY(button->props.onClick_ == events[i]);
  }
  ButtonNode *extra = 0;
  LOKA_VERIFY(ResolveSelector(h.scene, WithinAnchor("SimpleText.Ribbon").descendant<ButtonNode>(5), extra, error)
              != FLOW_STEP_SUCCEEDED);
  h.put("first.txt", "kept\nsecond");
  h.open("first.txt");
  LOKA_VERIFY(h.lines().update(h.lines().at(0).id, String("unsaved")) == EDIT_OK);
  scene::Scene *out = 0;
  LOKA_VERIFY(ClickButton(WithinAnchor("SimpleText.Ribbon").descendant<ButtonNode>(2)).run(h.scene, out, error)
              == FLOW_STEP_SUCCEEDED);
  h.flush();
  LOKA_VERIFY(SimpleTextTestAccess::operation(h.main()) == simpletext::OPEN);
  LOKA_VERIFY(find(&h.main(), "SimpleText.Open"));
  const ListRevision revision = h.lines().revision().get();
  const int refused[] = {1, 3, 4};
  for (int i = 0; i < 3; ++i)
  {
    LOKA_VERIFY(
        ClickButton(WithinAnchor("SimpleText.Ribbon").descendant<ButtonNode>(refused[i])).run(h.scene, out, error)
        == FLOW_STEP_SUCCEEDED);
    h.flush();
    LOKA_VERIFY(SimpleTextTestAccess::operation(h.main()) == simpletext::OPEN);
    LOKA_VERIFY(!(h.lines().revision().get() != revision));
    LOKA_VERIFY(h.row(0) == "unsaved" && h.bytes("first.txt") == "kept\nsecond");
    h.currentIs("first.txt");
  }
  h.choose(false, FileChooserResult::Canceled());
  LOKA_VERIFY(SimpleTextTestAccess::operation(h.main()) == simpletext::NONE);
  LOKA_VERIFY(!find(&h.main(), "SimpleText.Open"));
  LOKA_VERIFY(ClickButton(WithinAnchor("SimpleText.Ribbon").descendant<ButtonNode>(4)).run(h.scene, out, error)
              == FLOW_STEP_SUCCEEDED);
  h.flush();
  LOKA_VERIFY(SimpleTextTestAccess::operation(h.main()) == simpletext::SAVE);
  LOKA_VERIFY(find(&h.main(), "SimpleText.Save"));
  h.choose(true, FileChooserResult::Canceled());
  LOKA_VERIFY(SimpleTextTestAccess::operation(h.main()) == simpletext::NONE);
  LOKA_VERIFY(!find(&h.main(), "SimpleText.Save"));
  const ItemId old = h.lines().at(0).id;
  LOKA_VERIFY(ClickButton(WithinAnchor("SimpleText.Ribbon").descendant<ButtonNode>(1)).run(h.scene, out, error)
              == FLOW_STEP_SUCCEEDED);
  h.flush();
  LOKA_VERIFY(h.lines().size() == 1 && h.row(0).empty() && h.lines().at(0).id != old);
  LOKA_VERIFY(SimpleTextTestAccess::current(h.main()).kind == FileChooserResult::RESULT_NONE);
  LOKA_VERIFY(!h.hasError());
}

void testSimpleTextDetachWithdrawsFlows()
{
  Harness h;
  SimpleTextTestAccess::openEvent(h.config).emit();
  h.flush();
  loka::dsl::testing::SceneTestAccess::unmount(*h.scene);
  // Config emitters outlive the scene; terminal retirement must remove actions.
  SimpleTextTestAccess::newEvent(h.config).emit();
  SimpleTextTestAccess::openEvent(h.config).emit();
  SimpleTextTestAccess::saveEvent(h.config).emit();
  SimpleTextTestAccess::saveAsEvent(h.config).emit();
}
namespace
{
  struct LayoutProbe
  {
    explicit LayoutProbe(Harness &h)
        : harness(h),
          editor(),
          band(),
          calls(0)
    {
    }
    Harness &harness;
    scene::LayoutState editor;
    scene::LayoutState band;
    unsigned calls;
    static int columnChild(void *context, scene::Node *node, const scene::LayoutState &state)
    {
      LayoutProbe &probe = *static_cast<LayoutProbe *>(context);
      if (node->testId() == "SimpleText.Editor")
      {
        probe.editor = state;
        ++probe.calls;
        return state.y + state.height;
      }
      const int bottom = probe.harness.platform.projectLayoutForTesting(node, state);
      if (node->testId() == "SimpleText.Ribbon")
      {
        probe.band = state;
        probe.band.height = static_cast<short>(bottom - state.y);
      }
      return bottom;
    }
    static int rowChild(void *context, scene::Node *node, const scene::LayoutState &state)
    {
      if (node->asStackNode())
        return layout::computeColumnLayoutResultY(node->asStackNode(), state, context, &LayoutProbe::columnChild);
      return state.y;
    }
  };
} // namespace

void testSimpleTextEditorReceivesRemainingWindow()
{
  Harness h;
  scene::LayoutState viewport;
  viewport.x = 20;
  viewport.y = 30;
  viewport.width = 480;
  viewport.height = 320;
  viewport.lineHeight = 12;
  viewport.spacing = 4;
  for (int phase = 0; phase != 3; ++phase)
  {
    if (phase == 1)
    {
      SimpleTextTestAccess::openEvent(h.config).emit();
      h.flush();
    }
    if (phase == 2)
      h.choose(false, FileChooserResult::Error(5));
    LayoutProbe probe(h);
    StackNode *root = h.main().childrenHead()->asStackNode();
    LOKA_VERIFY(root && root->props.effectiveAxis() == STACK_AXIS_ROW);
    layout::RowLayoutMetrics metrics;
    metrics.gap = viewport.spacing;
    layout::computeRowLayoutResultY(root, viewport, metrics, &probe, &LayoutProbe::rowChild);
    LOKA_VERIFY(probe.calls == 1);
    LOKA_VERIFY(probe.band.height > 0);
    LOKA_VERIFY(probe.band.y + probe.band.height <= probe.editor.y);
    LOKA_VERIFY(probe.editor.x == viewport.x && probe.editor.width == viewport.width);
    LOKA_VERIFY(probe.editor.y > viewport.y);
    LOKA_VERIFY(probe.editor.height > 80);
    LOKA_VERIFY(probe.editor.y + probe.editor.height == viewport.y + viewport.height);
  }
}

#endif
