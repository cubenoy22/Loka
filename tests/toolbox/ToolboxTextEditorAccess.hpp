#ifndef LOKA_TEST_TOOLBOX_TEXT_EDITOR_ACCESS_HPP
#define LOKA_TEST_TOOLBOX_TEXT_EDITOR_ACCESS_HPP
#include "context/ToolboxTextEditorContext.hpp"
namespace loka
{
  namespace testing
  {
    /** Scenario/host observations only; no production getters or stored callbacks. */
    class ToolboxTextEditorAccess
    {
    public:
      static TEHandle te(ToolboxTextEditorContext &c)
      {
        return c.te_;
      }
      static unsigned restores(ToolboxTextEditorContext &c)
      {
        return c.restores_;
      }
      static app::EditorResult status(ToolboxTextEditorContext &c)
      {
        return c.status_;
      }
    };
  } // namespace testing
} // namespace loka
#endif
