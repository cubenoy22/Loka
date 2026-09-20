#ifndef LOKA_TESTS_SMIRK_BENCH_EDITOR_LINE_NODE_HPP
#define LOKA_TESTS_SMIRK_BENCH_EDITOR_LINE_NODE_HPP

#if !defined(TEST_BUILD)
#error SmirkBench editor-line specimen is scenario-only
#endif

#include "../../example/SmirkBench/src/MainNode.hpp"
#include "app/nodes/AttributedText.hpp"

namespace loka
{
  namespace scenario_tests
  {
    struct SmirkBenchEditorLineTypeTag
    {
    };
    class SmirkBenchEditorLineNode;

    /** Distinct props identity makes this declaration incompatible with ordinary MainNode. */
    struct SmirkBenchEditorLineProps : public app::scene::NodePropsBase<SmirkBenchEditorLineProps>
    {
      typedef SmirkBenchEditorLineTypeTag TypeTag;
      typedef SmirkBenchEditorLineNode NodeType;

      explicit SmirkBenchEditorLineProps(smirkbench::SmirkModel *model = 0)
          : main_(model)
      {
      }
      smirkbench::SmirkModel *model() const
      {
        return this->main_.model();
      }
      void assertInitialized() const
      {
        this->main_.assertInitialized();
      }
      bool operator<(const app::scene::PropsBase &rhs) const
      {
        return rhs.propsTypeId() == this->propsTypeId()
               && this->main_ < static_cast<const SmirkBenchEditorLineProps &>(rhs).main_;
      }

    private:
      smirkbench::MainProps main_;
    };

    /** The scenario type owns its editor State and always declares its specimen. */
    class SmirkBenchEditorLineNode : public smirkbench::MainNodeBase<SmirkBenchEditorLineProps>
    {
    public:
      explicit SmirkBenchEditorLineNode(const SmirkBenchEditorLineProps &props)
          : smirkbench::MainNodeBase<SmirkBenchEditorLineProps>(props)
      {
        using namespace app;
        this->state(this->editorLine_, Styled("var x = ", Bold) + Styled("1;", Italic));
      }

      void changeEditorLineForTesting()
      {
        using namespace app;
        this->editorLine_.set(Styled("var ", Bold) + Styled("x = ", TextStyle()) + Styled("1;", Italic));
      }

    protected:
      virtual void composeNavigation(app::StackDefinition &navPane)
      {
        using namespace app;
        smirkbench::MainNodeBase<SmirkBenchEditorLineProps>::composeNavigation(navPane);
        navPane << (Column() << Text("Editor line").TEST_ID("SmirkBench.PlainText")
                             << AttributedText(this->editorLine_.state()).TEST_ID("SmirkBench.EditorLine")
                             << (Box().size(28, 80).TEST_ID("SmirkBench.WrapBox")
                                 << (AttributedText(Styled("a ab", FontSize<12>() + Bold)
                                                    + Styled("cd", FontSize<24>() + Italic))
                                     + BlockStyle().wrap(TEXT_WRAP_WORD))
                                        .TEST_ID("SmirkBench.WrapFixture")));
      }

    private:
      app::scene::NodeState<app::AttributedString> editorLine_;
    };
  } // namespace scenario_tests
} // namespace loka
#endif
