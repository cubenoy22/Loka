#include "MyAppConfig.hpp"
#include "JsCardBindingRegistry.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullWindow.hpp"
#include "support/TestVerify.hpp"
#include "support/WindowAdmissionTestApp.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include <cstdio>
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

    // The native button takes reload() through the active card, validates the
    // replacement in a scratch runtime, and admits a replacement Scene.
    {
      NullScenePlatformController platform;
      WindowProps props;
      props.scene(smirkycard::CreateCard(SMIRKY_CARD_FIRST, loaded));
      NullWindow window(&context, props, &platform);
      WindowAdmissionTestApp admission(window);
      loka::dsl::testing::SceneTestAccess::updateAttached(*window.scene(), true);
      loka::app::scene::Scene *before = window.scene();
      writeMain(path, reloadCardSource("Reloaded First"));
      loka::app::scene::Node *reload = find(loka::dsl::testing::SceneTestAccess::rootNode(*before), "Reload");
      LOKA_VERIFY(reload && reload->asButtonNode());
      reload->asButtonNode()->props.getOnClick()->emit();
      LOKA_VERIFY(window.scene() == before);
      admission.flush();
      LOKA_VERIFY(window.scene() != before);
      loka::app::scene::Node *title =
          find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "SmirkyCard.Title");
      LOKA_VERIFY(title && title->asTextNode() && textValue(title->asTextNode()) == "Reloaded First");

      // A syntax error leaves the mounted card in place and writes its error
      // seat, rather than admitting an incomplete replacement.
      writeMain(path, "(");
      loka::app::scene::Scene *stable = window.scene();
      reload = find(loka::dsl::testing::SceneTestAccess::rootNode(*stable), "Reload");
      reload->asButtonNode()->props.getOnClick()->emit();
      LOKA_VERIFY(window.scene() == stable);
      loka::app::scene::Node *status =
          find(loka::dsl::testing::SceneTestAccess::rootNode(*stable), "SmirkyCard.Status");
      LOKA_VERIFY(status && status->asTextNode()
                  && textValue(status->asTextNode()).find("MAIN.JS:") != std::string::npos);

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
    }

    // A built-in card can reload a file which appears after startup.
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
                                    "target',()=>{}).TEST_ID('Target').enabled(this.on))}});",
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
    find(root, "Flip")->asButtonNode()->props.getOnClick()->emit();
    LOKA_VERIFY(!target->props.getEnabled()->get());
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

  void checkComposeRefusal(const char *source, const char *expected)
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
    loka::app::scene::Node *status =
        find(loka::dsl::testing::SceneTestAccess::rootNode(*window.scene()), "SmirkyCard.Status");
    LOKA_VERIFY(status && status->asTextNode());
    LOKA_VERIFY(textValue(status->asTextNode()).find(expected) != std::string::npos);
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

int main()
{
  checkRegistry();
  checkCounterAndRefusals();
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
