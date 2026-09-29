#include "MyAppConfig.hpp"
#include "JsCardBindingRegistry.hpp"
#include "SmirkyMarkup.hpp"
#include "JsClickNode.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "app/nodes/nestable/Grid.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/AttributedText.hpp"
#include "support/LokaAllocFailure.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullWindow.hpp"
#include "platform/file/FileIO.hpp"
#include "support/TestVerify.hpp"
#include "ScriptAlignedAlloc.h"
#include "ScriptEngine.h"
#include <stdint.h>
#include "support/WindowAdmissionTestApp.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
  class TestLowering : public smirkycard::IJsNodeLowering
  {
  public:
    explicit TestLowering(const char *value)
    {
      name = value;
      kind = 0;
    }
    virtual JSValue build(JSContext *, int, JSValueConst *)
    {
      return JS_UNDEFINED;
    }
    virtual loka::app::scene::NodeDefinitionBase *lower(smirkycard::JsCardNode &, JSContext *, JSValueConst, int)
    {
      return 0;
    }
  };
  void checkRegistry()
  {
    smirkycard::JsCardBindingRegistry registry;
    TestLowering *first = new TestLowering("first");
    LOKA_VERIFY(registry.registerLowering(first) && first->kind == 1);
    TestLowering *duplicateName = new TestLowering("first");
    LOKA_VERIFY(!registry.registerLowering(duplicateName));
    delete duplicateName;
    TestLowering *duplicateKind = new TestLowering("other");
    duplicateKind->kind = 9;
    LOKA_VERIFY(!registry.registerLowering(duplicateKind));
    delete duplicateKind;
    smirkycard::ScriptRuntime runtime;
    loka::core::String result, error;
    LOKA_VERIFY(runtime.evaluateToString(
        loka::core::String::Literal(
            "[VStack().kind,Text('').kind,EditText({}).kind,Button('',()=>{}).kind,Row().kind].join(',')"),
        result,
        error));
    LOKA_VERIFY(result.compare(loka::core::String::Literal("1,2,3,4,5")) == 0);
  }
  loka::app::scene::Node *find(loka::app::scene::Node *node, const char *id)
  {
    if (!node)
      return 0;
    if (node->testId() == id)
      return node;
    loka::app::scene::INestable *nestable = node->asNestable();
    for (loka::app::scene::Node *child = nestable ? nestable->childrenHead() : 0; child;
         child = child->nextInComposition)
    {
      loka::app::scene::Node *result = find(child, id);
      if (result)
        return result;
    }
    return 0;
  }

  void checkEvaluation(smirkycard::ScriptRuntime &runtime)
  {
    char error[256];
    LOKA_VERIFY(runtime.evaluate("['first', 'second'][1 + 0]", error, sizeof(error)) == SMIRKY_CARD_SECOND);
    LOKA_VERIFY(error[0] == '\0');
    const char *failures[] = {
        "(", "throw new Error('example failure')", "'unknown'", "42", "'first\\u0000extra'", "while (true) {}"};
    for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i)
    {
      LOKA_VERIFY(runtime.evaluate(failures[i], error, sizeof(error)) == SMIRKY_CARD_ERROR);
      LOKA_VERIFY(error[0] != '\0');
      // A failed/interrupting script must not poison the next evaluation.
      LOKA_VERIFY(runtime.evaluate("'first'", error, sizeof(error)) == SMIRKY_CARD_FIRST);
    }
    char tiny[1] = {'x'};
    LOKA_VERIFY(runtime.evaluate("42", tiny, sizeof(tiny)) == SMIRKY_CARD_ERROR);
    LOKA_VERIFY(tiny[0] == '\0');
    LOKA_VERIFY(SmirkyScriptEvaluate(0, "'first'", error, sizeof(error)) == SMIRKY_CARD_ERROR);
  }

  void checkSceneSwitching(smirkycard::ScriptRuntime &runtime)
  {
    NullPlatformContext context;
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);

    for (int i = 0; i < 20; ++i)
    {
      loka::app::scene::Scene *previous = window.scene();
      loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(*previous);
      loka::app::scene::Node *title = find(root, "SmirkyCard.Title");
      LOKA_VERIFY(title && title->asAttributedTextNode());
      LOKA_VERIFY(title->asAttributedTextNode()->props.text_->get()
                  == loka::app::Styled(i % 2 ? "Card Two" : "Card One", loka::app::FontSize<18>() + loka::app::Bold));
      loka::app::scene::Node *button = find(root, "SmirkyCard.Run");
      LOKA_VERIFY(button && button->asButtonNode());
      // Exercise the actual binding: C++ button -> JS -> SceneManager handoff.
      button->asButtonNode()->props.getOnClick()->emit();
      LOKA_VERIFY(window.scene() == previous);
      LOKA_VERIFY(window.sceneManager()->hasPendingReplacement());
      admission.flush();
      LOKA_VERIFY(window.scene() != previous);
      LOKA_VERIFY(!previous->getAttachedState()->get());
      LOKA_VERIFY(window.sceneManager()->hasRetiredScenes());

      // The replacement must already be mounted by the production path.
      LOKA_VERIFY(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()) != 0);
      admission.flush();
      LOKA_VERIFY(!window.sceneManager()->hasRetiredScenes());
    }
    // Window teardown exercises cleanup of the final mounted card as well.
  }

  std::string textValue(loka::app::TextNode *text)
  {
    const loka::core::StringBuffer buffer = text->props.text_->get().bufferWithEncoding(loka::core::StringEncodingUtf8);
    return std::string(static_cast<const char *>(buffer.data()), buffer.length());
  }

  bool makeDirectory(const char *path)
  {
#if defined(_WIN32)
    return _mkdir(path) == 0;
#else
    return mkdir(path, 0755) == 0;
#endif
  }

  bool removeDirectory(const char *path)
  {
#if defined(_WIN32)
    return _rmdir(path) == 0;
#else
    return rmdir(path) == 0;
#endif
  }

  void writeMain(const char *path, const std::string &text)
  {
    std::FILE *file = std::fopen(path, "wb");
    LOKA_VERIFY(file != 0);
    LOKA_VERIFY(std::fwrite(text.data(), 1, text.size(), file) == text.size());
    LOKA_VERIFY(std::fclose(file) == 0);
  }

  std::string reloadCardSource(const char *title)
  {
    return std::string("card('first',class{constructor(c){this.c=c;}compose(){return VStack(Text('") + title
           + "').TEST_ID('SmirkyCard.Title'),Button('r',()=>this.c.reload()).TEST_ID('Reload'),Text(this.c.error).TEST_"
             "ID("
             "'SmirkyCard.Status'))}});";
  }

  std::string constReloadCardSource(const char *title)
  {
    return std::string("const T='") + title
           + "';card('first',class{constructor(c){this.c=c;}compose(){return "
             "VStack(Text(T).TEST_ID('SmirkyCard.Title'),"
             "Button('r',()=>this.c.reload()).TEST_ID('Reload'),Text(this.c.error).TEST_ID('SmirkyCard.Status'))}});";
  }

  void printCurrentEngineMemory(smirkycard::ScriptRuntime &runtime)
  {
    JSMemoryUsage usage;
    JS_ComputeMemoryUsage(runtime.currentEngine()->jsRuntime(), &usage);
    std::printf("SmirkyCard QuickJS current after reload: malloc_size=%lld memory_used=%lld\n",
                static_cast<long long>(usage.malloc_size),
                static_cast<long long>(usage.memory_used_size));
  }

  std::string mountedTitle(smirkycard::ScriptRuntime &runtime, NullPlatformContext &context)
  {
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
    loka::app::scene::Node *title =
        find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "SmirkyCard.Title");
    if (title && title->asAttributedTextNode())
    {
      const loka::app::AttributedString &text = title->asAttributedTextNode()->props.text_->get();
      LOKA_VERIFY(text.segmentCount() == 1);
      const loka::core::StringBuffer buffer = text.segment(0).text.bufferWithEncoding(loka::core::StringEncodingUtf8);
      return std::string(static_cast<const char *>(buffer.data()), buffer.length());
    }
    return title && title->asTextNode() ? textValue(title->asTextNode()) : std::string();
  }

  std::string mountedStatus(smirkycard::ScriptRuntime &runtime, NullPlatformContext &context, SmirkyCardId card)
  {
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(card, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
    loka::app::scene::Node *status =
        find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "SmirkyCard.Status");
    return status && status->asTextNode() ? textValue(status->asTextNode()) : std::string();
  }

  void printScratchMemory()
  {
    smirkycard::ScriptRuntime scratch;
    loka::core::String error;
    LOKA_VERIFY(scratch.loadBuiltin(smirkycard::BuiltinMainJs(), error));
    JSMemoryUsage usage;
    JS_ComputeMemoryUsage(scratch.jsRuntime(), &usage);
    std::printf("SmirkyCard QuickJS scratch after built-in: malloc_size=%lld memory_used=%lld\n",
                static_cast<long long>(usage.malloc_size),
                static_cast<long long>(usage.memory_used_size));
  }

  void checkMainJsLoad()
  {
    printScratchMemory();
    const char *directory = "_smirkycard_main_fixture";
    const char *path = "_smirkycard_main_fixture/MAIN.JS";
    std::remove(path);
    removeDirectory(directory);
    LOKA_VERIFY(makeDirectory(directory));
    NullPlatformContext context;
    context.setApplicationDirectory(loka::core::String::Literal(directory));

    smirkycard::ScriptRuntime missing;
    missing.loadMain(&context);
    LOKA_VERIFY(missing.mainSource() == smirkycard::ScriptRuntime::MAIN_SOURCE_BUILTIN);
    LOKA_VERIFY(mountedTitle(missing, context) == "Card One");

    writeMain(path, reloadCardSource("Loaded First"));
    smirkycard::ScriptRuntime loaded;
    loaded.loadMain(&context);
    LOKA_VERIFY(loaded.mainSource() == smirkycard::ScriptRuntime::MAIN_SOURCE_FILE);
    LOKA_VERIFY(mountedTitle(loaded, context) == "Loaded First");

    // The native button takes c.reload() through its card context, admits the
    // evaluated candidate engine, and then admits a replacement Scene.
    {
      NullScenePlatformController platform;
      WindowProps props;
      props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, loaded));
      NullWindow window(&context, props, &platform);
      WindowAdmissionTestApp admission(window);
      loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
      loka::app::scene::Scene *before = window.scene();
      smirkycard::JsEngine *beforeEngine = loaded.currentEngine();
      writeMain(path, reloadCardSource("Reloaded First"));
      loka::app::scene::Node *reload = find(loka::dsl::testing::SceneTestAccess::rootNode(*before), "Reload");
      LOKA_VERIFY(reload && reload->asButtonNode());
      reload->asButtonNode()->props.getOnClick()->emit();
      LOKA_VERIFY(window.scene() == before);
      LOKA_VERIFY(loaded.currentEngine() != beforeEngine);
      LOKA_VERIFY(loaded.retiredEngineCount() == 1);
      admission.flush();
      LOKA_VERIFY(window.scene() != before);
      // The outgoing card may be reclaimed by this admission's unmount path;
      // in either case no generation survives beyond the following flush.
      LOKA_VERIFY(loaded.retiredEngineCount() <= 1);
      loka::app::scene::Node *title =
          find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "SmirkyCard.Title");
      LOKA_VERIFY(title && title->asTextNode() && textValue(title->asTextNode()) == "Reloaded First");
      printCurrentEngineMemory(loaded);
      admission.flush();
      LOKA_VERIFY(loaded.retiredEngineCount() == 0);

      // Each reload evaluates once in a fresh global scope, so a top-level
      // lexical declaration can be reloaded repeatedly without redeclaration.
      writeMain(path, constReloadCardSource("Const Reloaded"));
      reload = find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "Reload");
      reload->asButtonNode()->props.getOnClick()->emit();
      admission.flush();
      admission.flush();
      title = find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "SmirkyCard.Title");
      LOKA_VERIFY(title && title->asTextNode() && textValue(title->asTextNode()) == "Const Reloaded");
      writeMain(path, constReloadCardSource("Const Reloaded"));
      reload = find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "Reload");
      reload->asButtonNode()->props.getOnClick()->emit();
      admission.flush();
      admission.flush();
      title = find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "SmirkyCard.Title");
      LOKA_VERIFY(title && title->asTextNode() && textValue(title->asTextNode()) == "Const Reloaded");

      // Evaluation alone is not admission: the requested card must exist in
      // the candidate, or both the Scene and current engine stay unchanged.
      writeMain(path, "card('second',class{constructor(c){this.c=c;}compose(){return VStack()}});");
      loka::app::scene::Scene *stable = window.scene();
      smirkycard::JsEngine *stableEngine = loaded.currentEngine();
      reload = find(loka::dsl::testing::SceneTestAccess::rootNode(*stable), "Reload");
      reload->asButtonNode()->props.getOnClick()->emit();
      LOKA_VERIFY(window.scene() == stable);
      LOKA_VERIFY(loaded.currentEngine() == stableEngine);
      loka::app::scene::Node *status =
          find(loka::dsl::testing::SceneTestAccess::rootNode(*stable), "SmirkyCard.Status");
      LOKA_VERIFY(status && status->asTextNode()
                  && textValue(status->asTextNode()).find("MAIN.JS: card 'first' is not defined") != std::string::npos);

      // A syntax error leaves the mounted card in place and writes its error
      // seat, rather than admitting an incomplete replacement.
      writeMain(path, "(");
      reload = find(loka::dsl::testing::SceneTestAccess::rootNode(*stable), "Reload");
      reload->asButtonNode()->props.getOnClick()->emit();
      LOKA_VERIFY(window.scene() == stable);
      status = find(loka::dsl::testing::SceneTestAccess::rootNode(*stable), "SmirkyCard.Status");
      LOKA_VERIFY(status && status->asTextNode()
                  && textValue(status->asTextNode()).find("MAIN.JS:") != std::string::npos);

      // Reload evaluation is nested inside the old engine's button call, but
      // the candidate engine still receives the shared interrupt budget.
      writeMain(path, "for(;;){}");
      reload = find(loka::dsl::testing::SceneTestAccess::rootNode(*stable), "Reload");
      reload->asButtonNode()->props.getOnClick()->emit();
      LOKA_VERIFY(window.scene() == stable);
      status = find(loka::dsl::testing::SceneTestAccess::rootNode(*stable), "SmirkyCard.Status");
      LOKA_VERIFY(status && status->asTextNode()
                  && textValue(status->asTextNode()).find("interrupted") != std::string::npos);

      // A throwing candidate can mutate its scratch context, but never the
      // live one.
      writeMain(path, "globalThis.touched=1;throw new Error('reload broken');");
      reload = find(loka::dsl::testing::SceneTestAccess::rootNode(*stable), "Reload");
      reload->asButtonNode()->props.getOnClick()->emit();
      LOKA_VERIFY(window.scene() == stable);
      loka::core::String result, error;
      LOKA_VERIFY(loaded.evaluateToString(loka::core::String::Literal("String(globalThis.touched)"), result, error));
      LOKA_VERIFY(result.compare(loka::core::String::Literal("undefined")) == 0);

      LOKA_VERIFY(std::remove(path) == 0);
      reload = find(loka::dsl::testing::SceneTestAccess::rootNode(*stable), "Reload");
      reload->asButtonNode()->props.getOnClick()->emit();
      LOKA_VERIFY(window.scene() == stable);
      status = find(loka::dsl::testing::SceneTestAccess::rootNode(*stable), "SmirkyCard.Status");
      LOKA_VERIFY(status && status->asTextNode()
                  && textValue(status->asTextNode()).find("file is missing") != std::string::npos);

      // A registered card may still refuse while being constructed. Its
      // refusal tree retains a native reload door so editing the file recovers.
      writeMain(path,
                "card('first',class{constructor(c){this.c=c;throw new Error('constructor broken')}compose(){return "
                "VStack()}});");
      reload = find(loka::dsl::testing::SceneTestAccess::rootNode(*stable), "Reload");
      reload->asButtonNode()->props.getOnClick()->emit();
      admission.flush();
      loka::app::scene::Scene *refusal = window.scene();
      LOKA_VERIFY(refusal != stable);
      status = find(loka::dsl::testing::SceneTestAccess::rootNode(*refusal), "SmirkyCard.Status");
      LOKA_VERIFY(status && status->asTextNode()
                  && textValue(status->asTextNode()).find("Card constructor failed") != std::string::npos);
      admission.flush();
      writeMain(path, reloadCardSource("Recovered First"));
      reload = find(loka::dsl::testing::SceneTestAccess::rootNode(*refusal), "SmirkyCard.Reload");
      LOKA_VERIFY(reload && reload->asButtonNode());
      reload->asButtonNode()->props.getOnClick()->emit();
      admission.flush();
      title = find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "SmirkyCard.Title");
      LOKA_VERIFY(title && title->asTextNode() && textValue(title->asTextNode()) == "Recovered First");
      admission.flush();
      LOKA_VERIFY(loaded.retiredEngineCount() == 0);
    }

    // A built-in card can reload a file which appears after startup.
    LOKA_VERIFY(std::remove(path) == 0);
    smirkycard::ScriptRuntime builtin;
    builtin.loadMain(&context);
    LOKA_VERIFY(builtin.mainSource() == smirkycard::ScriptRuntime::MAIN_SOURCE_BUILTIN);
    {
      NullScenePlatformController platform;
      WindowProps props;
      props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, builtin));
      NullWindow window(&context, props, &platform);
      WindowAdmissionTestApp admission(window);
      loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
      writeMain(path, reloadCardSource("Appeared First"));
      loka::app::scene::Node *reload =
          find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "SmirkyCard.Reload");
      LOKA_VERIFY(reload && reload->asButtonNode());
      reload->asButtonNode()->props.getOnClick()->emit();
      admission.flush();
      loka::app::scene::Node *title =
          find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "SmirkyCard.Title");
      LOKA_VERIFY(title && title->asTextNode() && textValue(title->asTextNode()) == "Appeared First");
    }

    writeMain(path, "(");
    smirkycard::ScriptRuntime broken;
    broken.loadMain(&context);
    LOKA_VERIFY(broken.mainSource() == smirkycard::ScriptRuntime::MAIN_SOURCE_BUILTIN);
    LOKA_VERIFY(mountedTitle(broken, context) == "Card One");
    LOKA_VERIFY(mountedStatus(broken, context, SMIRKY_CARD_FIRST).find("MAIN.JS:") != std::string::npos);
    LOKA_VERIFY(mountedStatus(broken, context, SMIRKY_CARD_SECOND).find("MAIN.JS:") != std::string::npos);

    // A script that poisons a global before throwing must not reach the
    // fallback cards: the context is rebuilt before the built-in text runs.
    writeMain(path,
              "globalThis.Text = null; card('first', class{constructor(c){this.c=c;} compose() { return VStack(); } "
              "}); throw new "
              "Error('poison');");
    smirkycard::ScriptRuntime poisoned;
    poisoned.loadMain(&context);
    LOKA_VERIFY(poisoned.mainSource() == smirkycard::ScriptRuntime::MAIN_SOURCE_BUILTIN);
    LOKA_VERIFY(mountedTitle(poisoned, context) == "Card One");
    LOKA_VERIFY(mountedStatus(poisoned, context, SMIRKY_CARD_SECOND).find("poison") != std::string::npos);

    writeMain(path, std::string(64u * 1024u + 1u, 'x'));
    smirkycard::ScriptRuntime tooLarge;
    tooLarge.loadMain(&context);
    LOKA_VERIFY(mountedTitle(tooLarge, context) == "Card One");
    LOKA_VERIFY(mountedStatus(tooLarge, context, SMIRKY_CARD_FIRST).find("64 KiB") != std::string::npos);
    LOKA_VERIFY(mountedStatus(tooLarge, context, SMIRKY_CARD_SECOND).empty());
    LOKA_VERIFY(std::remove(path) == 0);
    LOKA_VERIFY(removeDirectory(directory));
  }

  std::string siblingCardSource(const char *title, const char *target)
  {
    return std::string("card('first',class{constructor(c){this.c=c;}compose(){return VStack(Text('") + title
           + "').TEST_ID('SmirkyCard.Title'),Button('open',()=>this.c.open('" + target
           + "')).TEST_ID('Open'),Button('reload',()=>this.c.reload()).TEST_ID('Reload'),"
             "Text(this.c.error).TEST_ID('SmirkyCard.Status'))}});";
  }

  loka::app::scene::Node *windowNode(NullWindow &window, const char *id)
  {
    return find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), id);
  }

  void clickCardButton(NullWindow &window, const char *id)
  {
    loka::app::scene::Node *button = windowNode(window, id);
    LOKA_VERIFY(button && button->asButtonNode());
    button->asButtonNode()->props.getOnClick()->emit();
  }

  std::string readCardSource(const char *name)
  {
    const std::string path = std::string(SMIRKYCARD_SOURCE_DIR) + "/" + name;
    std::FILE *file = loka::platform::file::OpenRead(loka::core::String::Utf8(path.data(), path.size()));
    LOKA_VERIFY(file != 0);
    std::string source;
    char buffer[4096];
    size_t count;
    while ((count = std::fread(buffer, 1, sizeof(buffer), file)) != 0)
      source.append(buffer, count);
    LOKA_VERIFY(!std::ferror(file));
    LOKA_VERIFY(std::fclose(file) == 0);
    return source;
  }

  loka::app::CellNode *mineCell(NullWindow &window, int index)
  {
    char id[32];
    std::sprintf(id, "Mines.Cell.%d", index);
    loka::app::scene::Node *node = windowNode(window, id);
    LOKA_VERIFY(node && node->asCellNode());
    return node->asCellNode();
  }

  std::string mineLabel(NullWindow &window, int index)
  {
    const loka::core::StringBuffer buffer =
        mineCell(window, index)->props.text_->get().bufferWithEncoding(loka::core::StringEncodingUtf8);
    return std::string(static_cast<const char *>(buffer.data()), buffer.length());
  }

  void clickMine(NullWindow &window, WindowAdmissionTestApp &admission, int index)
  {
    mineCell(window, index)->props.onClick_->emit();
    admission.flush();
  }

  std::string minesStatus(NullWindow &window)
  {
    loka::app::scene::Node *node = windowNode(window, "Mines.Status");
    LOKA_VERIFY(node && node->asTextNode());
    return textValue(node->asTextNode());
  }

  std::string mineSnapshot(NullWindow &window)
  {
    std::string result = minesStatus(window);
    for (int i = 0; i < 64; ++i)
      result += "|" + mineLabel(window, i);
    return result;
  }

  void checkMines()
  {
    const char *directory = "_smirkycard_mines_fixture";
    const char *mainPath = "_smirkycard_mines_fixture/MAIN.JS";
    const char *minesPath = "_smirkycard_mines_fixture/MINES.JS";
    std::remove(mainPath);
    std::remove(minesPath);
    removeDirectory(directory);
    LOKA_VERIFY(makeDirectory(directory));
    writeMain(mainPath, readCardSource("MAIN.JS"));
    writeMain(minesPath, "globalThis.SMIRKY_SEED = 0x13579BDF;\n" + readCardSource("MINES.JS"));
    NullPlatformContext context;
    context.setApplicationDirectory(loka::core::String::Literal(directory));
    smirkycard::ScriptRuntime runtime;
    runtime.loadMain(&context);
    {
      NullScenePlatformController platform;
      WindowProps props;
      props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
      NullWindow window(&context, props, &platform);
      WindowAdmissionTestApp admission(window);
      loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
      clickCardButton(window, "SmirkyCard.OpenMines");
      admission.flush();
      const int mines[3][10] = {{3, 4, 20, 22, 37, 45, 50, 55, 56, 59},
                                {1, 6, 16, 23, 27, 30, 34, 44, 45, 53},
                                {3, 26, 42, 49, 50, 56, 59, 60, 62, 63}};
      for (int board = 0; board < 3; ++board)
      {
        loka::app::scene::Node *content = windowNode(window, "Mines.Content");
        loka::app::scene::Node *grid = windowNode(window, "Mines.Board");
        LOKA_VERIFY(content && content->asStackNode());
        LOKA_VERIFY(content->asStackNode()->props.effectiveAxis() == loka::app::STACK_AXIS_COLUMN);
        LOKA_VERIFY(content->asNestable()->childrenCount() == 4);
        loka::app::scene::Node *status = content->asNestable()->childrenHead();
        LOKA_VERIFY(status == windowNode(window, "Mines.Status"));
        loka::app::scene::Node *error = status->nextInComposition;
        LOKA_VERIFY(error && error == windowNode(window, "SmirkyCard.Status") && error->asTextNode());
        LOKA_VERIFY(textValue(error->asTextNode()).empty());
        loka::app::scene::Node *controls = error->nextInComposition;
        LOKA_VERIFY(controls && controls->asStackNode());
        LOKA_VERIFY(controls->asStackNode()->props.effectiveAxis() == loka::app::STACK_AXIS_ROW);
        LOKA_VERIFY(controls->asNestable()->childrenCount() == 3);
        LOKA_VERIFY(find(controls, "Mines.Flag") && find(controls, "Mines.NewGame")
                    && find(controls, "SmirkyCard.OpenMain"));
        LOKA_VERIFY(grid && grid->asGridNode() && controls->nextInComposition == grid);
        LOKA_VERIFY(!grid->nextInComposition);
        LOKA_VERIFY(grid->asGridNode()->props.rows == 8 && grid->asGridNode()->props.cols == 8);
        LOKA_VERIFY(grid->asNestable()->childrenCount() == 64);
        LOKA_VERIFY(minesStatus(window) == "Mines left: 10");
        for (int i = 0; i < 64; ++i)
          LOKA_VERIFY(mineLabel(window, i) == ".");
        clickMine(window, admission, mines[board][0]);
        LOKA_VERIFY(minesStatus(window) == "Boom");
        for (int i = 0; i < 64; ++i)
        {
          bool expectedMine = false;
          for (int j = 0; j < 10; ++j)
            expectedMine = expectedMine || mines[board][j] == i;
          LOKA_VERIFY(mineLabel(window, i) == (expectedMine ? "X" : "."));
        }
        const std::string lost = mineSnapshot(window);
        for (int i = 0; i < 64; ++i)
          clickMine(window, admission, i);
        LOKA_VERIFY(mineSnapshot(window) == lost);
        if (board < 2)
        {
          clickCardButton(window, "Mines.NewGame");
          admission.flush();
        }
      }
      // Reopening the script restarts its seeded stream, unlike New Game.
      clickCardButton(window, "SmirkyCard.OpenMain");
      admission.flush();
      LOKA_VERIFY(windowNode(window, "SmirkyCard.Title")->asAttributedTextNode()->props.text_->get()
                  == loka::app::Styled("Card One", loka::app::FontSize<18>() + loka::app::Bold));
      clickCardButton(window, "SmirkyCard.OpenMines");
      admission.flush();
      clickCardButton(window, "Mines.Flag");
      admission.flush();
      LOKA_VERIFY(windowNode(window, "Mines.Flag")
                      ->asButtonNode()
                      ->props.getText()
                      ->get()
                      .compare(loka::core::String::Literal("Flag mode: on"))
                  == 0);
      clickMine(window, admission, 0);
      LOKA_VERIFY(mineLabel(window, 0) == "F" && minesStatus(window) == "Mines left: 9");
      clickCardButton(window, "Mines.Flag");
      admission.flush();
      const std::string flagged = mineSnapshot(window);
      clickMine(window, admission, 0);
      LOKA_VERIFY(mineSnapshot(window) == flagged);
      clickCardButton(window, "Mines.Flag");
      admission.flush();
      clickMine(window, admission, 0);
      LOKA_VERIFY(mineLabel(window, 0) == "." && minesStatus(window) == "Mines left: 10");
      // A flag inside the zero region must survive flood reveal.
      clickMine(window, admission, 8);
      clickCardButton(window, "Mines.Flag");
      admission.flush();
      clickMine(window, admission, 2);
      LOKA_VERIFY(mineLabel(window, 2) == "1");
      clickMine(window, admission, 0);
      LOKA_VERIFY(mineLabel(window, 0).empty());
      LOKA_VERIFY(mineLabel(window, 1).empty());
      LOKA_VERIFY(mineLabel(window, 16).empty());
      LOKA_VERIFY(mineLabel(window, 19) == "1");
      LOKA_VERIFY(mineLabel(window, 8) == "F");
      LOKA_VERIFY(mineLabel(window, 7) == "." && mineLabel(window, 63) == ".");
      clickCardButton(window, "Mines.Flag");
      admission.flush();
      const std::string revealed = mineSnapshot(window);
      clickMine(window, admission, 0);
      LOKA_VERIFY(mineSnapshot(window) == revealed);
      clickMine(window, admission, 8);
      clickCardButton(window, "Mines.Flag");
      admission.flush();
      LOKA_VERIFY(windowNode(window, "Mines.Flag")
                      ->asButtonNode()
                      ->props.getText()
                      ->get()
                      .compare(loka::core::String::Literal("Flag mode: off"))
                  == 0);
      for (int i = 0; i < 64; ++i)
      {
        bool isMine = false;
        for (int j = 0; j < 10; ++j)
          isMine = isMine || mines[0][j] == i;
        if (!isMine)
          clickMine(window, admission, i);
      }
      LOKA_VERIFY(minesStatus(window) == "You win");
      for (int i = 0; i < 64; ++i)
      {
        bool isMine = false;
        int adjacent = 0;
        for (int j = 0; j < 10; ++j)
        {
          isMine = isMine || mines[0][j] == i;
          const int dy = std::abs(mines[0][j] / 8 - i / 8);
          const int dx = std::abs(mines[0][j] % 8 - i % 8);
          if (dy <= 1 && dx <= 1 && (dx || dy))
            ++adjacent;
        }
        const std::string expected = isMine ? "." : adjacent ? std::string(1, '0' + adjacent) : "";
        LOKA_VERIFY(mineLabel(window, i) == expected);
      }
      const std::string won = mineSnapshot(window);
      for (int i = 0; i < 64; ++i)
        clickMine(window, admission, i);
      LOKA_VERIFY(mineSnapshot(window) == won);
      clickCardButton(window, "Mines.Flag");
      admission.flush();
      clickMine(window, admission, 3);
      LOKA_VERIFY(mineSnapshot(window) == won);
      // A failed open stays on the Mines card and names the file in its status.
      LOKA_VERIFY(std::remove(mainPath) == 0);
      clickCardButton(window, "SmirkyCard.OpenMain");
      admission.flush();
      LOKA_VERIFY(windowNode(window, "Mines.Board") != 0);
      const std::string failure = textValue(windowNode(window, "SmirkyCard.Status")->asTextNode());
      LOKA_VERIFY(failure.find("MAIN.JS") != std::string::npos);
    }
    LOKA_VERIFY(std::remove(minesPath) == 0);
    LOKA_VERIFY(removeDirectory(directory));
  }

  void checkOpenSibling()
  {
    const char *directory = "_smirkycard_sibling_fixture";
    const char *mainPath = "_smirkycard_sibling_fixture/MAIN.JS";
    const char *minesPath = "_smirkycard_sibling_fixture/MINES.JS";
    std::remove(mainPath);
    std::remove(minesPath);
    removeDirectory(directory);
    LOKA_VERIFY(makeDirectory(directory));
    NullPlatformContext context;
    context.setApplicationDirectory(loka::core::String::Literal(directory));
    writeMain(mainPath, siblingCardSource("Main", "./MINES.JS"));
    writeMain(minesPath, siblingCardSource("Mines", "./MAIN.JS"));
    smirkycard::ScriptRuntime runtime;
    runtime.loadMain(&context);
    {
      NullScenePlatformController platform;
      WindowProps props;
      props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
      NullWindow window(&context, props, &platform);
      WindowAdmissionTestApp admission(window);
      loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
      loka::app::scene::Scene *before = window.scene();
      smirkycard::JsEngine *engine = runtime.currentEngine();
      clickCardButton(window, "Open");
      LOKA_VERIFY(runtime.currentEngine() != engine);
      LOKA_VERIFY(window.scene() == before);
      LOKA_VERIFY(runtime.retiredEngineCount() == 1);
      admission.flush();
      LOKA_VERIFY(window.scene() != before);
      LOKA_VERIFY(textValue(windowNode(window, "SmirkyCard.Title")->asTextNode()) == "Mines");
      admission.flush();
      LOKA_VERIFY(runtime.retiredEngineCount() == 0);

      writeMain(minesPath, siblingCardSource("Edited Mines", "./MAIN.JS"));
      clickCardButton(window, "Reload");
      admission.flush();
      admission.flush();
      LOKA_VERIFY(textValue(windowNode(window, "SmirkyCard.Title")->asTextNode()) == "Edited Mines");
      writeMain(mainPath, siblingCardSource("Reopened Main", "MINES.JS"));
      clickCardButton(window, "Open");
      admission.flush();
      admission.flush();
      LOKA_VERIFY(textValue(windowNode(window, "SmirkyCard.Title")->asTextNode()) == "Reopened Main");
      writeMain(mainPath, siblingCardSource("Reloaded Main", "MINES.JS"));
      clickCardButton(window, "Reload");
      admission.flush();
      admission.flush();
      LOKA_VERIFY(textValue(windowNode(window, "SmirkyCard.Title")->asTextNode()) == "Reloaded Main");
      clickCardButton(window, "Open");
      admission.flush();
      admission.flush();
      LOKA_VERIFY(textValue(windowNode(window, "SmirkyCard.Title")->asTextNode()) == "Edited Mines");
      clickCardButton(window, "Open");
      admission.flush();
      admission.flush();

      const std::string failures[] = {
          "(", "card('second',class{constructor(c){this.c=c;}});", "for(;;){}", std::string(64u * 1024u + 1u, 'x')};
      const char *reasons[] = {"SyntaxError", "'first'", "interrupted", "64 KiB"};
      for (size_t i = 0; i <= sizeof(failures) / sizeof(failures[0]); ++i)
      {
        if (i < sizeof(failures) / sizeof(failures[0]))
          writeMain(minesPath, failures[i]);
        else
          LOKA_VERIFY(std::remove(minesPath) == 0);
        before = window.scene();
        engine = runtime.currentEngine();
        clickCardButton(window, "Open");
        LOKA_VERIFY(window.scene() == before && runtime.currentEngine() == engine);
        LOKA_VERIFY(!window.sceneManager()->hasPendingReplacement());
        const std::string status = textValue(windowNode(window, "SmirkyCard.Status")->asTextNode());
        LOKA_VERIFY(status.find("MINES.JS:") != std::string::npos);
        LOKA_VERIFY(status.find(i < sizeof(failures) / sizeof(failures[0]) ? reasons[i] : "missing")
                    != std::string::npos);
        LOKA_VERIFY(runtime.retiredEngineCount() == 0);
      }
    }
    // Each path refusal gets its own live card and exercises the actual JS door.
    const char *names[] = {
        "sub/MINES.JS", "../MINES.JS", "././MINES.JS", "MINES.JS\\u0000suffix", "MINES.JS:bad", "sub\\\\MINES.JS"};
    const char *displayNames[] = {
        "sub/MINES.JS", "../MINES.JS", "./MINES.JS", "MINES.JS", "MINES.JS:bad", "sub\\MINES.JS"};
    writeMain(minesPath, siblingCardSource("Must not open", "MAIN.JS"));
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
      writeMain(mainPath, siblingCardSource("Stable", names[i]));
      smirkycard::ScriptRuntime refused;
      refused.loadMain(&context);
      NullScenePlatformController platform;
      WindowProps props;
      props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, refused));
      NullWindow window(&context, props, &platform);
      WindowAdmissionTestApp admission(window);
      loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
      loka::app::scene::Scene *before = window.scene();
      smirkycard::JsEngine *engine = refused.currentEngine();
      clickCardButton(window, "Open");
      LOKA_VERIFY(window.scene() == before && refused.currentEngine() == engine);
      LOKA_VERIFY(!window.sceneManager()->hasPendingReplacement());
      LOKA_VERIFY(textValue(windowNode(window, "SmirkyCard.Status")->asTextNode()).find(displayNames[i])
                  != std::string::npos);
      admission.flush();
      LOKA_VERIFY(window.scene() == before && refused.currentEngine() == engine);
    }
    LOKA_VERIFY(std::remove(mainPath) == 0);
    LOKA_VERIFY(std::remove(minesPath) == 0);
    LOKA_VERIFY(removeDirectory(directory));
  }

  void printMemory(const char *phase, smirkycard::ScriptRuntime &runtime)
  {
    JSMemoryUsage usage;
    JS_ComputeMemoryUsage(runtime.jsRuntime(), &usage);
    std::printf("SmirkyCard QuickJS %s: malloc_size=%lld memory_used=%lld\n",
                phase,
                static_cast<long long>(usage.malloc_size),
                static_cast<long long>(usage.memory_used_size));
  }

  void checkCounterAndRefusals()
  {
    smirkycard::ScriptRuntime runtime;
    loka::core::String error;
    LOKA_VERIFY(
        runtime.loadBuiltin("card('first',class{constructor(c){this.c=c;this.count=this.c.state(0)}compose(){return "
                            "VStack(Button('+1',()=>{this.count.set(this.count.get()+1)}).TEST_ID('Counter."
                            "Increment'),Text(this.count).TEST_ID('Counter.Count'))}});",
                            error));
    NullPlatformContext context;
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
    printMemory("after compose", runtime);
    loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(*window.scene());
    loka::app::ButtonNode *increment = find(root, "Counter.Increment")->asButtonNode();
    loka::app::TextNode *count = find(root, "Counter.Count")->asTextNode();
    LOKA_VERIFY(increment && count);
    increment->props.getOnClick()->emit();
    increment->props.getOnClick()->emit();
    LOKA_VERIFY(textValue(count) == "2");

    NullPlatformContext secondContext;
    NullScenePlatformController secondPlatform;
    WindowProps secondProps;
    secondProps.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow secondWindow(&secondContext, secondProps, &secondPlatform);
    WindowAdmissionTestApp secondAdmission(secondWindow);
    loka::dsl::testing::SceneTestAccess::updateAttached(*secondWindow.scene(), true);
    printMemory("with two cards alive", runtime);
  }
  void checkManyClickables(const char *kind, int count = 70)
  {
    smirkycard::ScriptRuntime runtime;
    loka::core::String error;
    char length[16];
    std::sprintf(length, "%d", count);
    const std::string source = std::string("card('first',class{constructor(c){this.c=c;this.s=Array.from({length:")
                               + length
                               + "},(_,i)=>this.c.state('seat'+i))}"
                                 "compose(){return VStack(Array.from({length:Math.ceil(this.s.length/10)},(_,r)=>Row("
                                 "this.s.slice(r*10,r*10+10).map((s,j)=>"
                               + kind + "(s,()=>s.set('clicked'+(r*10+j))).TEST_ID('Click'+(r*10+j))))))}});";
    LOKA_VERIFY(runtime.loadBuiltin(source.c_str(), error));
    NullPlatformContext context;
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
    loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(*window.scene());
    loka::app::scene::Node *status = find(root, "SmirkyCard.Status");
    if (status)
      std::fprintf(stderr, "large card refusal: %s\n", textValue(status->asTextNode()).c_str());
    const int indices[] = {0, 8, count - 1};
    for (unsigned i = 0; i < sizeof(indices) / sizeof(indices[0]); ++i)
    {
      char id[32];
      std::sprintf(id, "Click%d", indices[i]);
      loka::app::scene::Node *node = find(root, id);
      LOKA_VERIFY(node);
      if (node->asButtonNode())
        node->asButtonNode()->props.getOnClick()->emit();
      else
        node->asCellNode()->props.onClick_->emit();
    }
    for (int i = 0; i < count; ++i)
    {
      char id[32], expected[32];
      std::sprintf(id, "Click%d", i);
      std::sprintf(expected, i == 0 || i == 8 || i == count - 1 ? "clicked%d" : "seat%d", i);
      loka::app::scene::Node *node = find(root, id);
      const loka::core::String value =
          node->asButtonNode() ? node->asButtonNode()->props.getText()->get() : node->asCellNode()->props.text_->get();
      LOKA_VERIFY(value.compare(loka::core::String::Utf8(expected, std::strlen(expected))) == 0);
    }
  }

  void checkClickableLifetime()
  {
    smirkycard::ScriptRuntime runtime;
    loka::core::String error, result;
    LOKA_VERIFY(runtime.loadBuiltin(
        "globalThis.clicks=0;card('first',class{constructor(c){this.c=c;}compose(){return VStack("
        "Button('go',()=>{++clicks;this.c.go('second')}).TEST_ID('Go'),"
        "Cell('stay',()=>{++clicks}).TEST_ID('Stay'))}});"
        "card('second',class{constructor(c){this.c=c;}compose(){return Text('second').TEST_ID('Second')}});",
        error));
    NullPlatformContext context;
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
    loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(*window.scene());
    loka::core::EmitterState *go = find(root, "Go")->asButtonNode()->props.getOnClick();
    loka::core::EmitterState *stay = find(root, "Stay")->asCellNode()->props.onClick_;
    stay->emit();
    go->emit();
    // Exercise withdrawal while storage is live: scene replacement's first
    // flush reclaims the root and its component arena, not just native contexts.
    loka::dsl::testing::SceneTestAccess::notifyComposeEvent(*window.scene(), loka::app::scene::COMPOSE_EVENT_DETACH);
    stay->emit();
    go->emit();
    LOKA_VERIFY(runtime.evaluateToString(loka::core::String::Literal("clicks"), result, error));
    LOKA_VERIFY(result.compare(loka::core::String::Literal("2")) == 0);
    admission.flush();
    LOKA_VERIFY(window.sceneManager()->hasRetiredScenes());
    LOKA_VERIFY(find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "Second"));
    admission.flush();
    LOKA_VERIFY(!window.sceneManager()->hasRetiredScenes());
  }

  void printRecordCosts()
  {
    const size_t seat = sizeof(smirkycard::JsSeatRecord);
    const size_t handler = sizeof(smirkycard::JsHandlerRecord);
    const size_t child = sizeof(smirkycard::JsClickNode);
    std::printf("SmirkyCard host costs: seat=%lu handler=%lu child=%lu 67/66=%lu bytes\n",
                static_cast<unsigned long>(seat),
                static_cast<unsigned long>(handler),
                static_cast<unsigned long>(child),
                static_cast<unsigned long>(67 * seat + 66 * (handler + child)));
  }

  void checkEnabledSeat()
  {
    smirkycard::ScriptRuntime runtime;
    loka::core::String error;
    LOKA_VERIFY(
        runtime.loadBuiltin("card('first',class{constructor(c){this.c=c;this.on=this.c.state(true)}compose(){return "
                            "VStack(Button('flip',()=>this.on.set(!this.on.get())).TEST_ID('Flip'),Button('"
                            "target',()=>{}).TEST_ID('Target').enabled(this.on),"
                            "Button('reverse',()=>{}).enabled(this.on).TEST_ID('Reverse'))}});",
                            error));
    NullPlatformContext context;
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
    loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(*window.scene());
    loka::app::scene::Node *targetNode = find(root, "Target");
    LOKA_VERIFY(targetNode);
    loka::app::ButtonNode *target = targetNode->asButtonNode();
    LOKA_VERIFY(target && target->props.getEnabled() && target->props.getEnabled()->get());
    loka::app::scene::Node *reverseNode = find(root, "Reverse");
    LOKA_VERIFY(reverseNode && reverseNode->asButtonNode());
    loka::app::ButtonNode *reverse = reverseNode->asButtonNode();
    LOKA_VERIFY(reverse->props.getEnabled() && reverse->props.getEnabled()->get());
    find(root, "Flip")->asButtonNode()->props.getOnClick()->emit();
    LOKA_VERIFY(!target->props.getEnabled()->get());
    LOKA_VERIFY(!reverse->props.getEnabled()->get());
  }
  void checkArrayChildren()
  {
    smirkycard::ScriptRuntime runtime;
    loka::core::String error;
    LOKA_VERIFY(runtime.loadBuiltin("card('first',class{constructor(c){this.c=c;}compose(){return "
                                    "VStack(['one','two','three'].map(v=>Text(v))).TEST_ID('Mapped')}});",
                                    error));
    NullPlatformContext context;
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
    loka::app::scene::INestable *mapped =
        find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "Mapped")->asNestable();
    int count = 0;
    for (loka::app::scene::Node *child = mapped->childrenHead(); child; child = child->nextInComposition)
      ++count;
    LOKA_VERIFY(count == 3);
  }
  void checkTreePropertyCopy()
  {
    smirkycard::ScriptRuntime runtime;
    loka::core::String result, error;
    LOKA_VERIFY(runtime.evaluateToString(
        loka::core::String::Literal(
            "(()=>{const tag=Text('').TEST_ID;const sym=Symbol('own');"
            "const source=Object.create({inherited:1});source.extra=undefined;source[sym]=7;"
            "Object.defineProperty(source,'hidden',{value:3});"
            "Object.defineProperty(source,'__proto__',{value:9,enumerable:true});"
            "const copy=tag.call(source,'t');"
            "return Object.hasOwn(copy,'extra') && copy.extra===undefined && copy[sym]===7 && "
            "!Object.hasOwn(copy,'inherited') && !Object.hasOwn(copy,'hidden') && "
            "Object.hasOwn(copy,'__proto__') && copy.__proto__===9 && "
            "copy.testId==='t' && Object.isFrozen(copy) && copy.TEST_ID('u').testId==='u';})()"),
        result,
        error));
    LOKA_VERIFY(result.compare(loka::core::String::Literal("true")) == 0);
    LOKA_VERIFY(runtime.evaluateToString(
        loka::core::String::Literal(
            "(()=>{try{Text('').TEST_ID.call({get extra(){throw new Error('copy getter')}},'t')}"
            "catch(e){return e.message==='copy getter'}return false})()"),
        result,
        error));
    LOKA_VERIFY(result.compare(loka::core::String::Literal("true")) == 0);
    // Invalid style must throw at the helper call, even when the tree is never lowered.
    LOKA_VERIFY(runtime.evaluateToString(
        loka::core::String::Literal(
            "(()=>{try{Text('a',{colour:1})}catch(e){return e instanceof TypeError}return false})()"),
        result,
        error));
    LOKA_VERIFY(result.compare(loka::core::String::Literal("true")) == 0);
    LOKA_VERIFY(runtime.evaluateToString(loka::core::String::Literal("typeof Text('a').enabled"), result, error));
    LOKA_VERIFY(result.compare(loka::core::String::Literal("undefined")) == 0);
  }

  void checkTextStyle()
  {
    using namespace loka::app;
    smirkycard::ScriptRuntime runtime;
    loka::core::String error;
    LOKA_VERIFY(runtime.loadBuiltin(
        "card('first',class{constructor(c){this.c=c;this.text=this.c.state('live')}compose(){return VStack("
        "Text('a',{size:24,weight:'bold',italic:true}).TEST_ID('Styled'),"
        "Text('a',{size:13}).TEST_ID('Snap13'),Text('a',{size:16}).TEST_ID('Snap16'),"
        "Text('a',{size:21}).TEST_ID('Snap21'),Text('a',{size:18}).TEST_ID('Exact18'),"
        "Text('a',{size:24}).TEST_ID('Chained'),"
        "Text('a',{weight:'normal',italic:false}).TEST_ID('Normal'),"
        "Text('a').TEST_ID('Plain'),Text(this.text,{size:18}).TEST_ID('Live'),"
        "Text(this.c.error,{italic:true}).TEST_ID('Error'))}});",
        error));
    NullPlatformContext context;
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
    loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(*window.scene());
    const char *ids[] = {
        "Chained", "Styled", "Snap13", "Snap16", "Snap21", "Exact18", "Normal", "Plain", "Live", "Error"};
    const TextStyle styles[] = {FontSize<24>(),
                                FontSize<24>() + Bold + Italic,
                                FontSize<12>(),
                                FontSize<14>(),
                                FontSize<18>(),
                                FontSize<18>(),
                                TextStyle().weight(TEXT_WEIGHT_NORMAL).italic(false),
                                TextStyle(),
                                FontSize<18>(),
                                Italic};
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i)
    {
      loka::app::scene::Node *node = find(root, ids[i]);
      LOKA_VERIFY(node && node->asTextNode());
      LOKA_VERIFY(node->asTextNode()->props.textStyle_ == styles[i]);
    }
    // #814: a Text with no style fields must not count as declared, or the
    // rails configure their native label and the goldens drift.
    loka::app::scene::Node *plain = find(root, "Plain");
    LOKA_VERIFY(plain && plain->asTextNode() && !plain->asTextNode()->props.hasDeclaredStyle());
  }

  void checkLifecycleHooks()
  {
    smirkycard::ScriptRuntime runtime;
    loka::core::String error, result;
    LOKA_VERIFY(runtime.loadBuiltin("card('first',class{constructor(c){this.c=c;this.value=this.c.state('new')}"
                                    "onAttach(){this.value.set('attached')}onDetach(){"
                                    "this.value.set('detached');globalThis.detached=this.value.get()}compose(){return "
                                    "VStack(Text(this.value).TEST_ID('HookValue'),Button('next',()=>this.c.go('second')"
                                    ").TEST_ID('HookNext'))}});card('"
                                    "second',class{constructor(c){this.c=c;}compose(){return VStack()}});",
                                    error));
    NullPlatformContext context;
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
    admission.flush();
    loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(*window.scene());
    LOKA_VERIFY(textValue(find(root, "HookValue")->asTextNode()) == "attached");
    find(root, "HookNext")->asButtonNode()->props.getOnClick()->emit();
    admission.flush();
    LOKA_VERIFY(runtime.evaluateToString(loka::core::String::Literal("detached"), result, error));
    LOKA_VERIFY(result.compare(loka::core::String::Literal("detached")) == 0);
    smirkycard::ScriptRuntime throwing;
    LOKA_VERIFY(throwing.loadBuiltin(
        "card('first',class{constructor(c){this.c=c;}onAttach(){throw new Error('attach boom')}compose(){return "
        "VStack(Text(this.c.error).TEST_ID('SmirkyCard.Status'))}});",
        error));
    WindowProps throwingProps;
    throwingProps.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, throwing));
    NullWindow throwingWindow(&context, throwingProps, &platform);
    WindowAdmissionTestApp throwingAdmission(throwingWindow);
    loka::dsl::testing::SceneTestAccess::updateAttached(*throwingWindow.scene(), true);
    throwingAdmission.flush();
    // A throwing hook lands in the error seat; the card itself stays mounted.
    loka::app::scene::Node *status =
        find(loka::dsl::testing::SceneTestAccess::rootNode(*throwingWindow.scene()), "SmirkyCard.Status");
    LOKA_VERIFY(status);
    LOKA_VERIFY(textValue(status->asTextNode()).find("attach boom") != std::string::npos);
  }

  void checkComposeRefusal(const char *source, const char *expected, bool consumesException = false)
  {
    smirkycard::ScriptRuntime runtime;
    loka::core::String error;
    LOKA_VERIFY(runtime.loadBuiltin(source, error));
    NullPlatformContext context;
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
    if (consumesException)
      LOKA_VERIFY(!JS_HasException(runtime.context()));
    loka::app::scene::Node *status =
        find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "SmirkyCard.Status");
    LOKA_VERIFY(!find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "Prefix"));
    LOKA_VERIFY(!find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "M"));
    LOKA_VERIFY(status && status->asTextNode());
    LOKA_VERIFY(textValue(status->asTextNode()).find(expected) != std::string::npos);
    loka::app::scene::Node *reload =
        find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "SmirkyCard.Reload");
    LOKA_VERIFY(reload && reload->asButtonNode() && reload->asButtonNode()->props.getOnClick());
  }

  void checkGridShape(int rows, int cols, bool clickable)
  {
    smirkycard::ScriptRuntime runtime;
    loka::core::String error;
    char prefix[128];
    std::sprintf(
        prefix,
        "card('first',class{constructor(c){this.c=c;}compose(){return Grid(%d,%d,Array.from({length:%d},(_,i)=>",
        rows,
        cols,
        rows * cols);
    const std::string source = std::string(prefix) + (clickable ? "Cell(String(i),()=>{})" : "Text(String(i))")
                               + ".TEST_ID(String(i)))).TEST_ID('Board')}});";
    LOKA_VERIFY(runtime.loadBuiltin(source.c_str(), error));
    NullPlatformContext context;
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
    loka::app::scene::Node *board = windowNode(window, "Board");
    LOKA_VERIFY(board && board->asGridNode());
    loka::app::GridNode *grid = board->asGridNode();
    LOKA_VERIFY(grid->props.rows == rows && grid->props.cols == cols);
    LOKA_VERIFY(grid->childrenCount() == static_cast<size_t>(rows * cols));
    int count = 0;
    for (loka::app::scene::Node *child = grid->childrenHead(); child; child = child->nextInComposition)
    {
      char expected[16];
      std::sprintf(expected, "%d", count++);
      loka::app::scene::Node *cell = find(child, expected);
      LOKA_VERIFY(cell);
      if (clickable)
      {
        LOKA_VERIFY(cell->asCellNode());
        LOKA_VERIFY(cell->asCellNode()->props.text_->get().compare(loka::core::String::Literal(expected)) == 0);
      }
      else
      {
        LOKA_VERIFY(cell->asTextNode());
        LOKA_VERIFY(textValue(cell->asTextNode()) == expected);
      }
    }
    LOKA_VERIFY(count == rows * cols);
  }

  void checkGrid()
  {
    checkGridShape(8, 8, true);
    // Text nodes cover the capacity without consuming the separate clickable budget.
    checkGridShape(16, 16, false);
    checkGridShape(1, 1, true);
    checkGridShape(2, 3, true);
    smirkycard::ScriptRuntime runtime;
    loka::core::String result, error;
    // Validation belongs to the helper call even if its result is never lowered.
    LOKA_VERIFY(!runtime.evaluateToString(loka::core::String::Literal("Grid(0,8,[]);'unused'"), result, error));
    const char *stacks[] = {"Row", "VStack"};
    for (size_t i = 0; i < sizeof(stacks) / sizeof(stacks[0]); ++i)
    {
      const std::string source = std::string("card('first',class{constructor(c){this.c=c;}compose(){return ")
                                 + stacks[i] + "(Array.from({length:17},()=>Text('x')))}});";
      checkComposeRefusal(source.c_str(), "accepts at most 16 children");
    }
    const char *badCounts[] = {"63", "65"};
    for (size_t i = 0; i < sizeof(badCounts) / sizeof(badCounts[0]); ++i)
    {
      const std::string source =
          std::string("card('first',class{constructor(c){this.c=c;}compose(){return Grid(8,8,Array.from({length:")
          + badCounts[i] + "},()=>Text('x')))}});";
      checkComposeRefusal(source.c_str(), "Grid requires exactly rows * cols children (64)");
    }
    const char *badDimensions[] = {"0", "17", "-1", "1.5", "NaN", "Infinity", "'8'", "null"};
    for (size_t i = 0; i < sizeof(badDimensions) / sizeof(badDimensions[0]); ++i)
      for (int axis = 0; axis < 2; ++axis)
      {
        const std::string source = std::string("card('first',class{constructor(c){this.c=c;}compose(){return Grid(")
                                   + (axis == 0 ? badDimensions[i] : "8") + "," + (axis == 1 ? badDimensions[i] : "8")
                                   + ",[])}});";
        checkComposeRefusal(source.c_str(), "Grid rows and cols must be integers in 1..16");
      }
    checkComposeRefusal("card('first',class{constructor(c){this.c=c;}compose(){return Grid(1,1,[[Text('x')]])}});",
                        "nested arrays");
    checkComposeRefusal("card('first',class{constructor(c){this.c=c;}compose(){return Grid(1,1)}});",
                        "Grid(rows, cols, children)");
    checkComposeRefusal("card('first',class{constructor(c){this.c=c;}compose(){const g=Grid(1,1,Text('x'));"
                        "g.children.pop();return g}});",
                        "Grid requires exactly rows * cols children");
    checkComposeRefusal("card('first',class{constructor(c){this.c=c;}compose(){const g=Grid(1,1,Text('x'));"
                        "g.children.push(Text('y'));return g}});",
                        "Grid requires exactly rows * cols children");
    checkComposeRefusal("card('first',class{constructor(c){this.c=c;}compose(){return "
                        "Object.assign({},Grid(1,1,Text('x')),{rows:0})}});",
                        "Grid rows and cols must be integers in 1..16");
    checkComposeRefusal("card('first',class{constructor(c){this.c=c;}compose(){return "
                        "Object.assign({},Grid(1,1,Text('x')),{cols:17})}});",
                        "Grid rows and cols must be integers in 1..16");
  }

  void checkTextStyleRefusals()
  {
    const char *styles[] = {"{colour:1}",
                            "{size:'big'}",
                            "{weight:1}",
                            "{weight:'heavy'}",
                            "{italic:1}",
                            "null",
                            "[]",
                            "new Date()",
                            "{size:13.5}",
                            "{size:NaN}",
                            "{size:Infinity}",
                            "{size:2147483648}",
                            "{[Symbol('x')]:1}",
                            "Object.defineProperty({},'colour',{value:1})",
                            "{italic:undefined}",
                            "{['size\\u0000']:24}",
                            "{weight:'bold\\u0000'}"};
    for (size_t i = 0; i < sizeof(styles) / sizeof(styles[0]); ++i)
    {
      const std::string source =
          std::string("card('first',class{constructor(c){this.c=c;}compose(){return Text('a',") + styles[i] + ")}});";
      checkComposeRefusal(source.c_str(), "TypeError");
    }
  }

  void checkChangedTextStyleRefusal()
  {
    // The tree owns a reference to the dictionary, so lowering must revalidate it.
    checkComposeRefusal("card('first',class{constructor(c){this.c=c;}compose(){const s={size:24};const t=Text('a',s);"
                        "s.colour=1;return t}});",
                        "TypeError");
    checkComposeRefusal("card('first',class{constructor(c){this.c=c;}compose(){return Text('a',{get size(){"
                        "throw new Error('style getter')}})}});",
                        "style getter");
  }

  void checkMarkupParser()
  {
    using namespace loka::app;
    AttributedString result;
    const TextStyle base = FontSize<12>();
    const char *sample = "var <b>x</b> = <i>1</i>;";
    LOKA_VERIFY(smirkycard::ParseSmirkyMarkup(sample, std::strlen(sample), base, result));
    LOKA_VERIFY(result
                == Styled("var ", base) + Styled("x", base + Bold) + Styled(" = ", base) + Styled("1", base + Italic)
                       + Styled(";", base));
    sample = "<size=24><b>a</b>b</size>";
    LOKA_VERIFY(smirkycard::ParseSmirkyMarkup(sample, std::strlen(sample), base, result));
    LOKA_VERIFY(result == Styled("a", FontSize<24>() + Bold) + Styled("b", FontSize<24>()));
    sample = "<size=24>a<size=12>b</size>c</size>d";
    LOKA_VERIFY(smirkycard::ParseSmirkyMarkup(sample, std::strlen(sample), base, result));
    LOKA_VERIFY(result
                == Styled("a", FontSize<24>()) + Styled("b", base) + Styled("c", FontSize<24>()) + Styled("d", base));
    sample = "\\<b>\\\\";
    LOKA_VERIFY(smirkycard::ParseSmirkyMarkup(sample, std::strlen(sample), base, result));
    LOKA_VERIFY(result == Styled("<b>\\", base));
    sample = "<size=21>x</size>";
    LOKA_VERIFY(smirkycard::ParseSmirkyMarkup(sample, std::strlen(sample), base, result));
    LOKA_VERIFY(result == Styled("x", FontSize<18>()));
    sample = "é<b>日本🙂</b>é";
    LOKA_VERIFY(smirkycard::ParseSmirkyMarkup(sample, std::strlen(sample), base, result));
    LOKA_VERIFY(result == Styled("é", base) + Styled("日本🙂", base + Bold) + Styled("é", base));
    sample = "<b></b><i></i>";
    LOKA_VERIFY(smirkycard::ParseSmirkyMarkup(sample, std::strlen(sample), base, result));
    LOKA_VERIFY(result.valid() && result.segmentCount() == 0);
    LOKA_VERIFY(smirkycard::ParseSmirkyMarkup(0, 0, base, result));
    LOKA_VERIFY(result.valid() && result.empty());
    const char *bad[] = {"<u>x</u>",
                         "<b>x",
                         "<b>x</i>",
                         "</b>",
                         "<size=>x</size>",
                         "<size=1x>x</size>",
                         "<size=2147483648>x</size>",
                         "<size=-1>x</size>",
                         "<size=1.5>x</size>",
                         "<size= 12>x</size>",
                         "<size=12",
                         "<>",
                         "</>",
                         "<b><i>x</b></i>",
                         "<size=12>x</size=12>"};
    const AttributedString sentinel = Styled("unchanged", base);
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
    {
      result = sentinel;
      LOKA_VERIFY(!smirkycard::ParseSmirkyMarkup(bad[i], std::strlen(bad[i]), base, result));
      LOKA_VERIFY(result == sentinel);
    }
    const char embedded[] = {'a', '\0', '<', 'b', '>', 'b', '<', '/', 'b', '>'};
    LOKA_VERIFY(smirkycard::ParseSmirkyMarkup(embedded, sizeof(embedded), base, result));
    LOKA_VERIFY(result == Styled(loka::core::String::Utf8(embedded, 2), base) + Styled("b", base + Bold));
    std::string deep;
    for (int i = 0; i < 1024; ++i)
      deep += "<b>";
    deep += "x";
    for (int i = 0; i < 1024; ++i)
      deep += "</b>";
    LOKA_VERIFY(smirkycard::ParseSmirkyMarkup(deep.data(), deep.size(), base, result));
    LOKA_VERIFY(result == Styled("x", base + Bold));
  }

  void checkMarkup()
  {
    using namespace loka::app;
    smirkycard::ScriptRuntime runtime;
    loka::core::String error, value;
    LOKA_VERIFY(runtime.evaluateToString(
        loka::core::String::Literal("(()=>{try{Markup('<b>x')}catch(e){return e instanceof TypeError}return false})()"),
        value,
        error));
    LOKA_VERIFY(value.compare(loka::core::String::Literal("true")) == 0);
    LOKA_VERIFY(runtime.loadBuiltin("card('first',class{constructor(c){this.c=c;}compose(){return VStack("
                                    "Markup('<b>Hi</b> there',{size:24}).TEST_ID('M'),"
                                    "Text('<b>plain</b>').TEST_ID('P'))}});",
                                    error));
    NullPlatformContext context;
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
    loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(*window.scene());
    loka::app::scene::Node *node = find(root, "M");
    LOKA_VERIFY(node && node->asAttributedTextNode());
    LOKA_VERIFY(node->asAttributedTextNode()->props.text_->get()
                == Styled("Hi", FontSize<24>() + Bold) + Styled(" there", FontSize<24>()));
    LOKA_VERIFY(textValue(find(root, "P")->asTextNode()) == "<b>plain</b>");
    checkComposeRefusal(
        "card('first',class{constructor(c){this.c=c;this.s=this.c.state('x')}compose(){return Markup(this.s)}});",
        "state seats are not supported");
    checkComposeRefusal("card('first',class{constructor(c){this.c=c;}compose(){return Markup('<b>x')}});", "TypeError");
    checkComposeRefusal("card('first',class{constructor(c){this.c=c;}compose(){return Markup('x',{colour:1})}});",
                        "TypeError");
    checkComposeRefusal("card('first',class{constructor(c){this.c=c;}compose(){const s={size:24};const t=Markup('x',s);"
                        "s.colour=1;return t}});",
                        "Markup",
                        true);
  }

  void checkBlockStyle()
  {
    using namespace loka::app;
    smirkycard::ScriptRuntime runtime;
    loka::core::String error, value;
    LOKA_VERIFY(runtime.loadBuiltin("card('first',class{constructor(c){this.c=c;this.s=this.c.state('s');this.n=this.c."
                                    "state(1)}compose(){return VStack("
                                    "Text('a',undefined,{align:'center'}).TEST_ID('T').TEST_ID('T'),"
                                    "Markup('<b>x</b>',{size:18},{align:'right',wrap:'word'}).TEST_ID('M'),"
                                    "Text(this.s,undefined,{wrap:'char',truncation:'clip'}).TEST_ID('S'),"
                                    "Text(this.n,undefined,{align:'left',truncation:'ellipsis'}).TEST_ID('N'),"
                                    "Text(this.c.error,undefined,{wrap:'none',truncation:'none'}).TEST_ID('E'),"
                                    "Text('default',undefined,undefined).TEST_ID('D'))}});",
                                    error));
    NullPlatformContext context;
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
    loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(*window.scene());
    const char *ids[] = {"T", "S", "N", "E", "D"};
    const BlockStyle expected[] = {BlockStyle().align(TEXT_ALIGN_CENTER),
                                   BlockStyle().wrap(TEXT_WRAP_CHAR).truncation(TEXT_TRUNCATION_CLIP),
                                   BlockStyle().align(TEXT_ALIGN_LEFT).truncation(TEXT_TRUNCATION_ELLIPSIS),
                                   BlockStyle().wrap(TEXT_WRAP_NONE).truncation(TEXT_TRUNCATION_NONE),
                                   BlockStyle()};
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i)
    {
      loka::app::scene::Node *node = find(root, ids[i]);
      LOKA_VERIFY(node && node->asTextNode());
      LOKA_VERIFY(node->asTextNode()->props.blockStyle_ == expected[i]);
    }
    loka::app::scene::Node *markup = find(root, "M");
    LOKA_VERIFY(markup && markup->asAttributedTextNode());
    LOKA_VERIFY(markup->asAttributedTextNode()->props.blockStyle_
                == BlockStyle().align(TEXT_ALIGN_RIGHT).wrap(TEXT_WRAP_WORD));
    LOKA_VERIFY(markup->asAttributedTextNode()->props.text_->get() == Styled("x", FontSize<18>() + Bold));

    const char *kinds[] = {"Text", "Markup"};
    const char *bad[] = {"{gap:4}",
                         "{align:1}",
                         "{align:'justify'}",
                         "{wrap:1}",
                         "{wrap:'words'}",
                         "{truncation:false}",
                         "{truncation:'cut'}",
                         "null",
                         "[]",
                         "new Date()",
                         "Object.create({align:'center'})",
                         "{[Symbol('x')]:1}",
                         "Object.defineProperty({},'gap',{value:4})",
                         "{align:undefined}",
                         "{align:'center\\u0000'}",
                         "{['align\\u0000']:'center'}"};
    for (size_t k = 0; k < sizeof(kinds) / sizeof(kinds[0]); ++k)
    {
      for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
      {
        const std::string call = std::string(kinds[k]) + "('a',undefined," + bad[i] + ")";
        const std::string source = "card('first',class{constructor(c){this.c=c;}compose(){return " + call + "}});";
        checkComposeRefusal(source.c_str(), "TypeError");
        const std::string caught = "(()=>{try{" + call + "}catch(e){return e instanceof TypeError}return false})()";
        LOKA_VERIFY(runtime.evaluateToString(loka::core::String::Utf8(caught.data(), caught.size()), value, error));
        LOKA_VERIFY(value.compare(loka::core::String::Literal("true")) == 0);
      }
      const std::string changed =
          std::string("card('first',class{constructor(c){this.c=c;}compose(){const b={align:'center'};const t=")
          + kinds[k] + "('a',undefined,b).TEST_ID('M');b.gap=4;return t}});";
      checkComposeRefusal(changed.c_str(), k == 0 ? "TypeError" : "Markup", true);
      const std::string throwing = std::string("card('first',class{constructor(c){this.c=c;}compose(){return ")
                                   + kinds[k] + "('a',undefined,{get align(){throw new Error('block getter')}})}});";
      checkComposeRefusal(throwing.c_str(), "block getter");
    }
    // Inherited properties are not dictionary entries, even on Object.prototype.
    LOKA_VERIFY(runtime.evaluateToString(
        loka::core::String::Literal(
            "(()=>{Object.prototype.gap=4;try{return Text('a',undefined,Object.create(null)).kind>0 && "
            "Markup('x',undefined,{}).kind>0}finally{delete Object.prototype.gap}})()"),
        value,
        error));
    LOKA_VERIFY(value.compare(loka::core::String::Literal("true")) == 0);
  }

  void checkMarkupAllocationRefusal()
  {
    using namespace loka::core::testing;
    // Two Builder allocations per pass for five segments: fail both initial
    // allocation and growth, first at declaration, then at lowering.
    for (int failure = 1; failure <= 4; ++failure)
    {
      failLokaAllocRaw("AttributedString", "Segments", failure);
      checkComposeRefusal(
          "card('first',class{constructor(c){this.c=c;}compose(){return VStack(Text('prefix').TEST_ID('Prefix'),"
          "Markup('a<b>b</b>c<i>d</i>e').TEST_ID('M'))}});",
          failure <= 2 ? "TypeError" : "Markup",
          true);
      LOKA_VERIFY(lokaAllocRawLive() == 0);
      allowLokaAllocRaw();
    }
    failLokaAllocRaw("AttributedString", "Segments", 1);
    {
      loka::app::AttributedString result;
      LOKA_VERIFY(!smirkycard::ParseSmirkyMarkup(0, 0, loka::app::TextStyle(), result));
      LOKA_VERIFY(result.valid() && result.empty());
    }
    LOKA_VERIFY(lokaAllocRawLive() == 0);
    allowLokaAllocRaw();
    failLokaAllocRaw("SmirkyMarkup", "Frame", 2);
    {
      loka::app::AttributedString result;
      const char *sample = "<b>a<i>b</i></b>";
      LOKA_VERIFY(!smirkycard::ParseSmirkyMarkup(sample, std::strlen(sample), loka::app::TextStyle(), result));
      LOKA_VERIFY(result.empty());
    }
    LOKA_VERIFY(lokaAllocRawLive() == 0);
    allowLokaAllocRaw();
  }

  void checkRecordAllocationRefusal()
  {
    using namespace loka::core::testing;
    const char *types[] = {"Seat", "Handler"};
    for (unsigned i = 0; i < sizeof(types) / sizeof(types[0]); ++i)
    {
      failLokaAllocRaw("JsCardNode", types[i], 1);
      checkComposeRefusal("card('first',class{constructor(c){this.c=c;this.s=this.c.state('s')}compose(){return "
                          "Button(this.s,()=>{})}});",
                          "Could not allocate");
      LOKA_VERIFY(lokaAllocRawLive() == 0);
      allowLokaAllocRaw();
    }
  }

  void checkRequiredRefusals()
  {
    checkComposeRefusal(
        "card('first',class{constructor(c){this.c=c;}compose(){this.c.state('late');return VStack()}});",
        "constructor");
    checkComposeRefusal("card('first',class{constructor(c){this.c=c;this.c.state(1.5)}compose(){return VStack()}});",
                        "state(number) requires an integer");
    checkComposeRefusal("card('first',class{constructor(c){this.c=c;}get compose(){for(;;){}}});", "interrupted");
    checkComposeRefusal("card('first',class{constructor(c){this.c=c;for(let "
                        "i=0;i<129;++i)this.c.state(0)}compose(){return VStack()}});",
                        "kCardSeatBudget");
    checkComposeRefusal(
        "card('first',class{constructor(c){this.c=c;}compose(){return VStack(Array.from({length:9},(_,r)=>Row("
        "Array.from({length:r==8?1:16},()=>Button('x',()=>{})))))}});",
        "kCardClickableBudget");
    checkComposeRefusal(
        "card('first',class{constructor(c){this.c=c;}compose(){return VStack(Array.from({length:9},(_,r)=>Row("
        "Array.from({length:r==8?1:16},()=>Cell('x',()=>{})))))}});",
        "kCardClickableBudget");
    checkComposeRefusal("card('first',class{constructor(c){this.c=c;}compose(){return {kind:99}}});", "unknown kind");

    checkComposeRefusal(
        "card('first',class{constructor(c){this.c=c;}compose(){return VStack(Array.from({length:9},(_,r)=>Row("
        "Array.from({length:r==8?1:16},()=>r%2?Cell('x',()=>{}):Button('x',()=>{})))))}});",
        "kCardClickableBudget");
    checkComposeRefusal(
        "card('first',class{constructor(c){this.c=c;this.s=this.c.state(1)}compose(){return Cell(this.s,()=>{})}});",
        "String state seat");
    checkComposeRefusal("card('first',class{constructor(c){this.c=c;}compose(){return Cell('x',42)}});", "function");

    smirkycard::ScriptRuntime runtime;
    loka::core::String error;
    LOKA_VERIFY(runtime.loadBuiltin("card('first',class{constructor(c){this.c=c;}compose(){return "
                                    "VStack(Button('Throw',()=>{throw new Error('handler "
                                    "boom')}).TEST_ID('Throw'),Button('Loop',()=>{for(;;){}}).TEST_ID('Loop'),Text("
                                    "this.c.error).TEST_ID('SmirkyCard."
                                    "Status'))}});",
                                    error));
    NullPlatformContext context;
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
    loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(*window.scene());
    loka::app::TextNode *status = find(root, "SmirkyCard.Status")->asTextNode();
    find(root, "Throw")->asButtonNode()->props.getOnClick()->emit();
    LOKA_VERIFY(textValue(status).find("handler boom") != std::string::npos);
    find(root, "Loop")->asButtonNode()->props.getOnClick()->emit();
    LOKA_VERIFY(textValue(status).find("interrupted") != std::string::npos);

    smirkycard::ScriptRuntime exceptionRuntime;
    LOKA_VERIFY(exceptionRuntime.loadBuiltin(
        "card('first',class{constructor(c){this.c=c;this.value=this.c.state('ready')}compose(){return "
        "VStack(Button('Throw "
        "object',()=>{throw "
        "{toString(){for(;;){}}}}).TEST_ID('ObjectThrow'),Button('Recover',()=>this.value.set('working')).TEST_ID('"
        "Recover'),Text(this.value).TEST_ID('Value'),Text(this.c.error).TEST_ID('SmirkyCard.Status'))}});",
        error));
    NullPlatformContext exceptionContext;
    NullScenePlatformController exceptionPlatform;
    WindowProps exceptionProps;
    exceptionProps.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, exceptionRuntime));
    NullWindow exceptionWindow(&exceptionContext, exceptionProps, &exceptionPlatform);
    WindowAdmissionTestApp exceptionAdmission(exceptionWindow);
    loka::dsl::testing::SceneTestAccess::updateAttached(*exceptionWindow.scene(), true);
    loka::app::scene::Node *exceptionRoot = loka::dsl::testing::SceneTestAccess::rootNode(*exceptionWindow.scene());
    find(exceptionRoot, "ObjectThrow")->asButtonNode()->props.getOnClick()->emit();
    LOKA_VERIFY(textValue(find(exceptionRoot, "SmirkyCard.Status")->asTextNode()).find("interrupted")
                != std::string::npos);
    find(exceptionRoot, "Recover")->asButtonNode()->props.getOnClick()->emit();
    LOKA_VERIFY(textValue(find(exceptionRoot, "Value")->asTextNode()) == "working");
  }

  void expectJs(smirkycard::ScriptRuntime &runtime, const char *source, const char *expected)
  {
    loka::core::String result, error;
    LOKA_VERIFY(runtime.evaluateToString(loka::core::String::Literal(source), result, error));
    LOKA_VERIFY(result.compare(loka::core::String::Literal(expected)) == 0);
  }

  void checkCardContext()
  {
    std::puts("[pin] card context: class, factory, arrow construction exactly once");
    const char *factories[] = {"class {constructor(c){++calls;if(c.error.get()!=='')throw Error('early error "
                               "seat');this.s=c.state('class');this.c=c;globalThis.context=c;}"
                               "compose(d){globalThis.delegate=d;return Text(this.s).TEST_ID('Value')}}",
                               "function(c){++calls;globalThis.context=c;var s=c.state('factory');return {"
                               "compose(d){globalThis.delegate=d;return Text(s).TEST_ID('Value')}}}",
                               "(c)=>{++calls;globalThis.context=c;var s=c.state('arrow');return {"
                               "compose(d){globalThis.delegate=d;return Text(s).TEST_ID('Value')}}}"};
    const char *values[] = {"class", "factory", "arrow"};
    for (unsigned i = 0; i < 3; ++i)
    {
      smirkycard::ScriptRuntime runtime;
      loka::core::String error;
      const std::string source = std::string("var calls=0;card('first',") + factories[i] + ");";
      LOKA_VERIFY(runtime.loadBuiltin(source.c_str(), error));
      NullPlatformContext context;
      NullScenePlatformController platform;
      WindowProps props;
      props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
      NullWindow window(&context, props, &platform);
      WindowAdmissionTestApp admission(window);
      loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
      loka::app::scene::Node *valueNode = windowNode(window, "Value");
      LOKA_VERIFY(valueNode && valueNode->asTextNode());
      LOKA_VERIFY(textValue(valueNode->asTextNode()) == values[i]);
      expectJs(runtime, "calls", "1");
      expectJs(runtime,
               "[typeof state,typeof go,typeof open,typeof reload].join(',')",
               "undefined,undefined,undefined,undefined");
      expectJs(runtime, "try{delegate.declare(VStack());'accepted'}catch(e){String(e).includes('only valid')}", "true");
      expectJs(runtime, "context.state('late')", "undefined");
      expectJs(runtime, "context.error.get().includes('constructor')", "true");
    }
    {
      smirkycard::ScriptRuntime runtime;
      loka::core::String error;
      LOKA_VERIFY(runtime.loadBuiltin("var reads=0;card('first',c=>({get compose(){++reads;return d=>{"
                                      "globalThis.failedDeclare=d.declare;throw Error('compose threw')}},"
                                      "get onAttach(){++reads;return ()=>{}},get onDetach(){++reads;return ()=>{}}}));",
                                      error));
      NullPlatformContext context;
      NullScenePlatformController platform;
      WindowProps props;
      props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
      NullWindow window(&context, props, &platform);
      WindowAdmissionTestApp admission(window);
      loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
      LOKA_VERIFY(textValue(windowNode(window, "SmirkyCard.Status")->asTextNode()).find("compose threw")
                  != std::string::npos);
      expectJs(runtime, "reads", "3");
      expectJs(runtime, "try{failedDeclare(VStack());'accepted'}catch(e){String(e).includes('only valid')}", "true");
    }
    std::puts("[pin] card context: malformed results and throwing property reads");
    const char *bad[] = {
        "function(c){return {}}", "function(c){return 1}", "function(c){}", "c=>1", "c=>undefined", "c=>({compose:1})"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i)
    {
      const std::string source = std::string("card('first',") + bad[i] + ");";
      checkComposeRefusal(source.c_str(), "card(name, F): F(c) must return an object with compose()");
    }
    checkComposeRefusal("card('first',c=>({get compose(){throw Error('compose read')}}));", "compose read");
    checkComposeRefusal("card('first',c=>({compose(){return VStack()},get onAttach(){throw Error('attach read')}}));",
                        "attach read");
    checkComposeRefusal("card('first',c=>({compose(){return VStack()},get onDetach(){throw Error('detach read')}}));",
                        "detach read");
  }

  void checkExceptionContext()
  {
    std::puts("[pin] card context: exception stringification cannot reenter closed windows");
    const char *factories[] = {
        "c=>{throw {toString(){globalThis.late=String(c.state(1));return 'constructor throw'}}}",
        "c=>({compose(d){throw {toString(){try{d.declare(Text('late'));globalThis.late='accepted'}"
        "catch(e){globalThis.late='refused'}return 'compose throw'}}}})",
        "c=>{var s=c.state('live');return {compose(){return VStack()},onDetach(){throw {toString(){"
        "try{s.get();globalThis.late='accepted'}catch(e){globalThis.late='refused'}return 'detach throw'}}}}}"};
    for (unsigned i = 0; i < 3; ++i)
    {
      smirkycard::ScriptRuntime runtime;
      loka::core::String error;
      const std::string source = std::string("var late='unset';card('first',") + factories[i] + ");";
      LOKA_VERIFY(runtime.loadBuiltin(source.c_str(), error));
      NullPlatformContext context;
      NullScenePlatformController platform;
      WindowProps props;
      props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
      NullWindow window(&context, props, &platform);
      WindowAdmissionTestApp admission(window);
      loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
      if (i == 2)
        loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), false);
      expectJs(runtime, "late", i == 0 ? "undefined" : "refused");
    }
  }

  void checkContextRevocation()
  {
    std::puts("[pin] card context: TransitionPending, synchronous revocation, compose scope");
    smirkycard::ScriptRuntime runtime;
    loka::core::String error;
    LOKA_VERIFY(runtime.loadBuiltin(
        "var secondRefused=false,detachValue='',detachGo=false,clicks=0;"
        "card('first',function(c){var seat=c.state('live');"
        "globalThis.saved=seat;globalThis.savedError=c.error;globalThis.savedContext=c;"
        "globalThis.savedGo=c.go;globalThis.savedState=c.state;"
        "return {compose(d){globalThis.savedDeclare=d.declare;return VStack("
        "Button('next',()=>{++clicks;c.go('second');try{c.go('first')}catch(e){secondRefused=true}}).TEST_ID('Next'),"
        "Text(c.error).TEST_ID('Error'))},"
        "onDetach(){seat.set('detaching');detachValue=seat.get();"
        "try{c.go('first')}catch(e){detachGo=true}throw Error('detach hook threw')}}});"
        "card('second',c=>({compose(){return Text('second').TEST_ID('Second')}}));",
        error));
    NullPlatformContext context;
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
    expectJs(runtime, "savedError.get()", "");
    expectJs(runtime, "try{savedDeclare(VStack());'accepted'}catch(e){String(e).includes('only valid')}", "true");
    windowNode(window, "Next")->asButtonNode()->props.getOnClick()->emit();
    expectJs(runtime, "secondRefused", "true");
    // Detach without reclaim: the capability must close on this line, even if the hook throws.
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), false);
    expectJs(runtime, "detachValue+','+detachGo", "detaching,true");
    const char *retained[] = {"savedGo('first')",
                              "savedState(1)",
                              "saved.get()",
                              "saved.set('bad')",
                              "savedError.get()",
                              "savedContext.reload()",
                              "savedContext.open('MAIN.JS')",
                              "savedDeclare(VStack())"};
    for (unsigned i = 0; i < sizeof(retained) / sizeof(retained[0]); ++i)
    {
      const std::string probe = std::string("try{") + retained[i] + ";'accepted'}catch(e){'refused'}";
      expectJs(runtime, probe.c_str(), "refused");
    }
    admission.flush();
    admission.flush();
    LOKA_VERIFY(windowNode(window, "Second") != 0);
    expectJs(runtime, "try{saved.get();'accepted'}catch(e){'refused'}", "refused");
  }

  void checkRetiredSeatAndIntegerRefusal()
  {
    smirkycard::ScriptRuntime runtime;
    loka::core::String error;
    LOKA_VERIFY(runtime.loadBuiltin("card('first',class{constructor(c){this.c=c;this.count=this.c.state(0);globalThis."
                                    "saved=this.count}compose(){return "
                                    "VStack(Button('Next',()=>this.c.go('second')).TEST_ID('Next'))}});card('second',"
                                    "class{constructor(c){this.c=c;}compose(){"
                                    "return "
                                    "VStack()}});",
                                    error));
    NullPlatformContext context;
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
    find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "Next")
        ->asButtonNode()
        ->props.getOnClick()
        ->emit();
    admission.flush();
    admission.flush();
    loka::core::String result;
    LOKA_VERIFY(!runtime.evaluateToString(loka::core::String::Literal("saved.get()"), result, error));
    LOKA_VERIFY(!runtime.evaluateToString(loka::core::String::Literal("saved.set(1)"), result, error));
    const loka::core::StringBuffer retiredError = error.bufferWithEncoding(loka::core::StringEncodingUtf8);
    LOKA_VERIFY(std::string(static_cast<const char *>(retiredError.data()), retiredError.length()).find("revoked card")
                != std::string::npos);

    smirkycard::ScriptRuntime integerRuntime;
    LOKA_VERIFY(integerRuntime.loadBuiltin(
        "card('first',class{constructor(c){this.c=c;this.count=this.c.state(1);globalThis.numberSeat=this.count}"
        "compose(){return "
        "VStack(Text(this.count).TEST_ID('Number.Count'),Text(this.c.error).TEST_ID('SmirkyCard.Status'))}});",
        error));
    NullPlatformContext integerContext;
    NullScenePlatformController integerPlatform;
    WindowProps integerProps;
    integerProps.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, integerRuntime));
    NullWindow integerWindow(&integerContext, integerProps, &integerPlatform);
    WindowAdmissionTestApp integerAdmission(integerWindow);
    loka::dsl::testing::SceneTestAccess::updateAttached(*integerWindow.scene(), true);
    loka::app::scene::Node *integerRoot = loka::dsl::testing::SceneTestAccess::rootNode(*integerWindow.scene());
    LOKA_VERIFY(integerRuntime.evaluateToString(loka::core::String::Literal("numberSeat.set(1.5)"), result, error));
    LOKA_VERIFY(textValue(find(integerRoot, "Number.Count")->asTextNode()) == "1");
    LOKA_VERIFY(textValue(find(integerRoot, "SmirkyCard.Status")->asTextNode()).find("integer") != std::string::npos);
    LOKA_VERIFY(integerRuntime.evaluateToString(loka::core::String::Literal("numberSeat.set(2)"), result, error));
    LOKA_VERIFY(textValue(find(integerRoot, "Number.Count")->asTextNode()) == "2");
  }

  void checkMessageBox(smirkycard::ScriptRuntime &runtime)
  {
    NullPlatformContext context;
    NullScenePlatformController platform;
    WindowProps props;
    props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, runtime));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp admission(window);
    loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);

    loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(*window.scene());
    loka::app::scene::Node *scriptNode = find(root, "SmirkyCard.Script");
    loka::app::scene::Node *runNode = find(root, "SmirkyCard.RunScript");
    loka::app::scene::Node *resultNode = find(root, "SmirkyCard.Result");
    LOKA_VERIFY(scriptNode && runNode && resultNode);
    loka::app::EditTextNode *script = scriptNode->asEditTextNode();
    loka::app::ButtonNode *run = runNode->asButtonNode();
    loka::app::TextNode *result = resultNode->asTextNode();
    LOKA_VERIFY(script && run && result && script->props.text_.isValid() && run->props.getOnClick());

    loka::app::scene::BoundaryNode *owner = loka::dsl::testing::SceneTestAccess::rootBoundary(*window.scene());
    const char *sources[] = {"1+1",
                             "['a','b'][1].toUpperCase()",
                             "throw new Error('x')",
                             "for(;;){}",
                             "1+1",
                             "this === globalThis",
                             "typeof this.script"};
    const char *expected[] = {"2", "B", 0, 0, "2", "true", "undefined"};
    for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); ++i)
    {
      {
        loka::core::StateTrackerGuard guard(owner->tracker());
        script->props.text_.set(loka::core::String::Literal(sources[i]));
      }
      run->props.getOnClick()->emit();
      const std::string value = textValue(result);
      if (expected[i])
        LOKA_VERIFY(value == expected[i]);
      else if (i == 2)
        LOKA_VERIFY(value.find("Error:") == 0 && value.find(i == 2 ? "x" : "interrupted") != std::string::npos);
      else
      {
        loka::app::scene::Node *status = find(root, "SmirkyCard.Status");
        LOKA_VERIFY(status && textValue(status->asTextNode()).find("interrupted") != std::string::npos);
      }
    }
    // The interrupt budget also covers stringification: a result whose
    // toString loops must come back as an interrupted error, not a hang.
    {
      {
        loka::core::StateTrackerGuard guard(owner->tracker());
        script->props.text_.set(loka::core::String::Literal("({toString: function(){ for(;;){} }})"));
      }
      run->props.getOnClick()->emit();
      loka::app::scene::Node *status = find(root, "SmirkyCard.Status");
      LOKA_VERIFY(status && textValue(status->asTextNode()).find("interrupted") != std::string::npos);
    }
    // Embedded NULs survive (the seam returns byte counts, not strlen).
    {
      {
        loka::core::StateTrackerGuard guard(owner->tracker());
        script->props.text_.set(loka::core::String::Literal("'a\\u0000b'"));
      }
      run->props.getOnClick()->emit();
      const std::string value = textValue(result);
      LOKA_VERIFY(value.size() == 3 && value[0] == 'a' && value[1] == '\0' && value[2] == 'b');
    }
  }
} // namespace

namespace
{
  // A base allocator that hands out 2-byte-aligned blocks, the Mac Plus
  // Memory Manager's alignment (#837). Each block is a host malloc shifted by
  // two bytes so the shift is visible and the raw pointer can be recovered.
  int oddBlocksLive = 0;
  void *TwoByteAlignedAllocate(size_t size)
  {
    char *raw = static_cast<char *>(std::malloc(size + 2));
    if (!raw)
      return 0;
    ++oddBlocksLive;
    return raw + 2;
  }
  void TwoByteAlignedRelease(void *block)
  {
    --oddBlocksLive;
    std::free(static_cast<char *>(block) - 2);
  }

  void testScriptAllocatorAlignsTwoByteAlignedBase()
  {
    std::printf("[pin] testScriptAllocatorAlignsTwoByteAlignedBase\n");
    const SmirkyBaseAllocator base = {TwoByteAlignedAllocate, TwoByteAlignedRelease};
    for (size_t size = 1; size <= 96; size += 7)
    {
      void *block = SmirkyAlignedAllocate(&base, size);
      LOKA_VERIFY(block != 0);
      // QuickJS tags the low two bits of a context pointer and lays arenas
      // out with 8-byte members: nothing below 8 is enough.
      LOKA_VERIFY((reinterpret_cast<uintptr_t>(block) & (SMIRKY_SCRIPT_ALLOC_ALIGN - 1)) == 0);
      LOKA_VERIFY(SmirkyAlignedUsableSize(block) == size);
      std::memset(block, 0x5a, size);
      void *grown = SmirkyAlignedReallocate(&base, block, size + 40);
      LOKA_VERIFY(grown != 0);
      LOKA_VERIFY((reinterpret_cast<uintptr_t>(grown) & (SMIRKY_SCRIPT_ALLOC_ALIGN - 1)) == 0);
      LOKA_VERIFY(SmirkyAlignedUsableSize(grown) == size + 40);
      LOKA_VERIFY(static_cast<unsigned char *>(grown)[size - 1] == 0x5a);
      void *shrunk = SmirkyAlignedReallocate(&base, grown, size);
      LOKA_VERIFY(shrunk == grown && SmirkyAlignedUsableSize(shrunk) == size);
      SmirkyAlignedRelease(&base, shrunk);
    }
    void *fromNull = SmirkyAlignedReallocate(&base, 0, 8);
    LOKA_VERIFY(fromNull != 0 && (reinterpret_cast<uintptr_t>(fromNull) & 7) == 0);
    LOKA_VERIFY(SmirkyAlignedReallocate(&base, fromNull, 0) == 0);
    SmirkyAlignedRelease(&base, 0);
    LOKA_VERIFY(oddBlocksLive == 0);
    LOKA_VERIFY(SmirkyAlignedUsableSize(0) == 0);
  }

  void testScriptContextIsPointerTagAligned()
  {
    std::printf("[pin] testScriptContextIsPointerTagAligned\n");
    SmirkyScript *script = SmirkyScriptCreate();
    LOKA_VERIFY(script != 0);
    LOKA_VERIFY((reinterpret_cast<uintptr_t>(SmirkyScriptContext(script)) & 3) == 0);
    LOKA_VERIFY((reinterpret_cast<uintptr_t>(SmirkyScriptRuntime(script)) & (SMIRKY_SCRIPT_ALLOC_ALIGN - 1)) == 0);
    SmirkyScriptDestroy(script);
  }
} // namespace

int main(int argc, char **argv)
{
  if (argc == 2 && !std::strcmp(argv[1], "--exception-context"))
  {
    checkExceptionContext();
    return 0;
  }
  if (argc == 2 && !std::strcmp(argv[1], "--card-context"))
  {
    checkCardContext();
    checkContextRevocation();
    checkExceptionContext();
    return 0;
  }
  if (argc == 2 && !std::strcmp(argv[1], "--clickable-lifetime"))
  {
    checkClickableLifetime();
    return 0;
  }
  if (argc == 2 && !std::strcmp(argv[1], "--many-clickables"))
  {
    checkManyClickables("Button");
    checkManyClickables("Cell");
    return 0;
  }
  if (argc == 2 && !std::strcmp(argv[1], "--grid"))
  {
    checkGrid();
    return 0;
  }
  if (argc == 2 && !std::strcmp(argv[1], "--mines"))
  {
    checkMines();
    return 0;
  }
  checkCardContext();
  checkContextRevocation();
  checkExceptionContext();
  checkGrid();
  checkMines();
  testScriptAllocatorAlignsTwoByteAlignedBase();
  testScriptContextIsPointerTagAligned();
  checkRegistry();
  checkCounterAndRefusals();
  checkTextStyle();
  checkMarkupParser();
  checkMarkup();
  checkBlockStyle();
  checkMarkupAllocationRefusal();
  checkTreePropertyCopy();
  checkTextStyleRefusals();
  checkChangedTextStyleRefusal();
  printRecordCosts();
  checkClickableLifetime();
  checkManyClickables("Button");
  checkManyClickables("Cell");
  checkManyClickables("Cell", 128);
  checkEnabledSeat();
  checkArrayChildren();
  checkLifecycleHooks();
  smirkycard::ScriptRuntime runtime;
  loka::core::String builtinError;
  LOKA_VERIFY(runtime.loadBuiltin(smirkycard::BuiltinMainJs(), builtinError));
  checkEvaluation(runtime);
  checkMessageBox(runtime);
  checkSceneSwitching(runtime);
  checkRecordAllocationRefusal();
  checkRequiredRefusals();
  checkRetiredSeatAndIntegerRefusal();
  checkMainJsLoad();
  checkOpenSibling();
  LOKA_VERIFY(!smirkycard::CreateCard(SMIRKY_CARD_ERROR, runtime));
  std::puts("SmirkyCard: message box, evaluation, failure recovery, and 20 Scene switches passed.");
  return 0;
}
