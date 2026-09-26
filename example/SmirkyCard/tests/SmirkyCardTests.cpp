#include "MyAppConfig.hpp"
#include "JsCardBindingRegistry.hpp"
#include "SmirkyMarkup.hpp"
#include "app/nodes/AttributedText.hpp"
#include "support/LokaAllocFailure.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullWindow.hpp"
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
      LOKA_VERIFY(title && title->asTextNode());
      LOKA_VERIFY(
          title->asTextNode()->props.text_->get().compare(loka::core::String::Literal(i % 2 ? "Card Two" : "Card One"))
          == 0);
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
    return std::string("card('first',class{constructor(){}compose(){return VStack(Text('") + title
           + "').TEST_ID('SmirkyCard.Title'),Button('r',()=>reload()).TEST_ID('Reload'),Text(this.error).TEST_ID("
             "'SmirkyCard.Status'))}});";
  }

  std::string constReloadCardSource(const char *title)
  {
    return std::string("const T='") + title
           + "';card('first',class{constructor(){}compose(){return VStack(Text(T).TEST_ID('SmirkyCard.Title'),"
             "Button('r',()=>reload()).TEST_ID('Reload'),Text(this.error).TEST_ID('SmirkyCard.Status'))}});";
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

    // The native button takes reload() through the active card, admits the
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
      writeMain(path, "card('second',class{constructor(){}compose(){return VStack()}});");
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
                "card('first',class{constructor(){throw new Error('constructor broken')}compose(){return "
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
              "globalThis.Text = null; card('first', class { compose() { return VStack(); } }); throw new "
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
    LOKA_VERIFY(runtime.loadBuiltin("card('first',class{constructor(){this.count=state(0)}compose(){return "
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
  void checkEnabledSeat()
  {
    smirkycard::ScriptRuntime runtime;
    loka::core::String error;
    LOKA_VERIFY(runtime.loadBuiltin("card('first',class{constructor(){this.on=state(true)}compose(){return "
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
    LOKA_VERIFY(runtime.loadBuiltin("card('first',class{constructor(){}compose(){return "
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
    LOKA_VERIFY(runtime.loadBuiltin("card('first',class{constructor(){this.text=state('live')}compose(){return VStack("
                                    "Text('a',{size:24,weight:'bold',italic:true}).TEST_ID('Styled'),"
                                    "Text('a',{size:13}).TEST_ID('Snap13'),Text('a',{size:16}).TEST_ID('Snap16'),"
                                    "Text('a',{size:21}).TEST_ID('Snap21'),Text('a',{size:18}).TEST_ID('Exact18'),"
                                    "Text('a',{size:24}).TEST_ID('Chained'),"
                                    "Text('a',{weight:'normal',italic:false}).TEST_ID('Normal'),"
                                    "Text('a').TEST_ID('Plain'),Text(this.text,{size:18}).TEST_ID('Live'),"
                                    "Text(this.error,{italic:true}).TEST_ID('Error'))}});",
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
    LOKA_VERIFY(runtime.loadBuiltin(
        "card('first',class{constructor(){this.value=state('new')}onAttach(){this.value.set('attached')}onDetach(){"
        "this.value.set('detached');globalThis.detached=this.value.get()}compose(){return "
        "VStack(Text(this.value).TEST_ID('HookValue'),Button('next',()=>go('second')).TEST_ID('HookNext'))}});card('"
        "second',class{constructor(){}compose(){return VStack()}});",
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
    LOKA_VERIFY(throwing.loadBuiltin("card('first',class{onAttach(){throw new Error('attach boom')}compose(){return "
                                     "VStack(Text(this.error).TEST_ID('SmirkyCard.Status'))}});",
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
      const std::string source = std::string("card('first',class{compose(){return Text('a',") + styles[i] + ")}});";
      checkComposeRefusal(source.c_str(), "TypeError");
    }
  }

  void checkChangedTextStyleRefusal()
  {
    // The tree owns a reference to the dictionary, so lowering must revalidate it.
    checkComposeRefusal("card('first',class{compose(){const s={size:24};const t=Text('a',s);"
                        "s.colour=1;return t}});",
                        "TypeError");
    checkComposeRefusal("card('first',class{compose(){return Text('a',{get size(){"
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
    LOKA_VERIFY(runtime.loadBuiltin("card('first',class{compose(){return VStack("
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
    checkComposeRefusal("card('first',class{constructor(){this.s=state('x')}compose(){return Markup(this.s)}});",
                        "state seats are not supported");
    checkComposeRefusal("card('first',class{compose(){return Markup('<b>x')}});", "TypeError");
    checkComposeRefusal("card('first',class{compose(){return Markup('x',{colour:1})}});", "TypeError");
    checkComposeRefusal("card('first',class{compose(){const s={size:24};const t=Markup('x',s);"
                        "s.colour=1;return t}});",
                        "Markup",
                        true);
  }

  void checkMarkupAllocationRefusal()
  {
    using namespace loka::core::testing;
    // Two Builder allocations per pass for five segments: fail both initial
    // allocation and growth, first at declaration, then at lowering.
    for (int failure = 1; failure <= 4; ++failure)
    {
      failLokaAllocRaw("AttributedString", "Segments", failure);
      checkComposeRefusal("card('first',class{compose(){return VStack(Text('prefix').TEST_ID('Prefix'),"
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

  void checkRequiredRefusals()
  {
    checkComposeRefusal("card('first',class{constructor(){}compose(){state('late');return VStack()}});", "constructor");
    checkComposeRefusal("card('first',class{constructor(){state(1.5)}compose(){return VStack()}});",
                        "state(number) requires an integer");
    checkComposeRefusal("card('first',class{constructor(){}get compose(){for(;;){}}});", "interrupted");
    checkComposeRefusal(
        "card('first',class{constructor(){this.a=state(0);this.b=state(0);this.c=state(0);this.d=state(0);this.e=state("
        "0);this.f=state(0);this.g=state(0);this.h=state(0);this.i=state(0)}compose(){return VStack()}});",
        "maximum 8");
    checkComposeRefusal("card('first',class{constructor(){}compose(){return {kind:99}}});", "unknown kind");

    smirkycard::ScriptRuntime runtime;
    loka::core::String error;
    LOKA_VERIFY(runtime.loadBuiltin(
        "card('first',class{constructor(){}compose(){return VStack(Button('Throw',()=>{throw new Error('handler "
        "boom')}).TEST_ID('Throw'),Button('Loop',()=>{for(;;){}}).TEST_ID('Loop'),Text(this.error).TEST_ID('SmirkyCard."
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
        "card('first',class{constructor(){this.value=state('ready')}compose(){return VStack(Button('Throw "
        "object',()=>{throw "
        "{toString(){for(;;){}}}}).TEST_ID('ObjectThrow'),Button('Recover',()=>this.value.set('working')).TEST_ID('"
        "Recover'),Text(this.value).TEST_ID('Value'),Text(this.error).TEST_ID('SmirkyCard.Status'))}});",
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

  void checkRetiredSeatAndIntegerRefusal()
  {
    smirkycard::ScriptRuntime runtime;
    loka::core::String error;
    LOKA_VERIFY(runtime.loadBuiltin(
        "card('first',class{constructor(){this.count=state(0);globalThis.saved=this.count}compose(){return "
        "VStack(Button('Next',()=>go('second')).TEST_ID('Next'))}});card('second',class{constructor(){}compose(){"
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
    LOKA_VERIFY(runtime.evaluateToString(loka::core::String::Literal("saved.get()"), result, error));
    LOKA_VERIFY(result.compare(loka::core::String::Literal("undefined")) == 0);
    LOKA_VERIFY(!runtime.evaluateToString(loka::core::String::Literal("saved.set(1)"), result, error));
    const loka::core::StringBuffer retiredError = error.bufferWithEncoding(loka::core::StringEncodingUtf8);
    LOKA_VERIFY(std::string(static_cast<const char *>(retiredError.data()), retiredError.length()).find("retired card")
                != std::string::npos);

    smirkycard::ScriptRuntime integerRuntime;
    LOKA_VERIFY(integerRuntime.loadBuiltin(
        "card('first',class{constructor(){this.count=state(1);globalThis.numberSeat=this.count}compose(){return "
        "VStack(Text(this.count).TEST_ID('Number.Count'),Text(this.error).TEST_ID('SmirkyCard.Status'))}});",
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
    const char *sources[] = {"1+1", "['a','b'][1].toUpperCase()", "throw new Error('x')", "for(;;){}", "1+1"};
    const char *expected[] = {"2", "B", 0, 0, "2"};
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

int main()
{
  testScriptAllocatorAlignsTwoByteAlignedBase();
  testScriptContextIsPointerTagAligned();
  checkRegistry();
  checkCounterAndRefusals();
  checkTextStyle();
  checkMarkupParser();
  checkMarkup();
  checkMarkupAllocationRefusal();
  checkTreePropertyCopy();
  checkTextStyleRefusals();
  checkChangedTextStyleRefusal();
  checkEnabledSeat();
  checkArrayChildren();
  checkLifecycleHooks();
  smirkycard::ScriptRuntime runtime;
  loka::core::String builtinError;
  LOKA_VERIFY(runtime.loadBuiltin(smirkycard::BuiltinMainJs(), builtinError));
  checkEvaluation(runtime);
  checkMessageBox(runtime);
  checkSceneSwitching(runtime);
  checkRequiredRefusals();
  checkRetiredSeatAndIntegerRefusal();
  checkMainJsLoad();
  LOKA_VERIFY(!smirkycard::CreateCard(SMIRKY_CARD_ERROR, runtime));
  std::puts("SmirkyCard: message box, evaluation, failure recovery, and 20 Scene switches passed.");
  return 0;
}
