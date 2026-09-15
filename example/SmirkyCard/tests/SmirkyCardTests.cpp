#include "MyAppConfig.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullWindow.hpp"
#include "support/TestVerify.hpp"
#include "support/WindowAdmissionTestApp.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include <cstdio>
#include <cstring>
#include <string>

namespace
{
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
    LOKA_VERIFY(script && run && result && script->props.text_ && run->props.getOnClick());

    loka::app::scene::BoundaryNode *owner = loka::dsl::testing::SceneTestAccess::rootBoundary(*window.scene());
    const char *sources[] = {"1+1", "['a','b'][1].toUpperCase()", "throw new Error('x')", "for(;;){}", "1+1"};
    const char *expected[] = {"2", "B", 0, 0, "2"};
    for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); ++i)
    {
      {
        loka::core::StateTrackerGuard guard(owner->tracker());
        script->props.text_->set(loka::core::String::Literal(sources[i]));
      }
      run->props.getOnClick()->emit();
      const std::string value = textValue(result);
      if (expected[i])
        LOKA_VERIFY(value == expected[i]);
      else
        LOKA_VERIFY(value.find("Error:") == 0 && value.find(i == 2 ? "x" : "interrupted") != std::string::npos);
    }
    // The interrupt budget also covers stringification: a result whose
    // toString loops must come back as an interrupted error, not a hang.
    {
      {
        loka::core::StateTrackerGuard guard(owner->tracker());
        script->props.text_->set(loka::core::String::Literal("({toString: function(){ for(;;){} }})"));
      }
      run->props.getOnClick()->emit();
      const std::string value = textValue(result);
      LOKA_VERIFY(value.find("Error:") == 0 && value.find("interrupted") != std::string::npos);
    }
    // Embedded NULs survive (the seam returns byte counts, not strlen).
    {
      {
        loka::core::StateTrackerGuard guard(owner->tracker());
        script->props.text_->set(loka::core::String::Literal("'a\\u0000b'"));
      }
      run->props.getOnClick()->emit();
      const std::string value = textValue(result);
      LOKA_VERIFY(value.size() == 3 && value[0] == 'a' && value[1] == '\0' && value[2] == 'b');
    }
    // A result over the 511-byte cap is cut on a UTF-8 boundary: 200 x U+3042
    // (3 bytes each) keeps 170 whole characters = 510 bytes.
    {
      {
        loka::core::StateTrackerGuard guard(owner->tracker());
        script->props.text_->set(loka::core::String::Literal("'\\u3042'.repeat(200)"));
      }
      run->props.getOnClick()->emit();
      const std::string value = textValue(result);
      LOKA_VERIFY(value.size() == 510 && static_cast<unsigned char>(value[507]) == 0xE3u
                  && static_cast<unsigned char>(value[508]) == 0x81u && static_cast<unsigned char>(value[509]) == 0x82u);
    }
  }
} // namespace

int main()
{
  smirkycard::ScriptRuntime runtime;
  checkEvaluation(runtime);
  checkMessageBox(runtime);
  checkSceneSwitching(runtime);
  std::puts("SmirkyCard: message box, evaluation, failure recovery, and 20 Scene switches passed.");
  return 0;
}
