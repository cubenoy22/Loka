#include "app/nodes/controls/TextEditor.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"

namespace seam_key_pin
{
  using namespace loka::app;
  using namespace loka::app::scene;

  class EditorContext : public NodeContext
  {
  public:
    EditorContext(TextEditorNode *node, const SeamKey<TextEditorNode> &key)
    {
      (void)node->seam(key);
    }
    void readLifecycleFactOnAttach() {}
  };

  class EditorHandler : public RetainedNodeHandler<EditorHandler, TextEditorNode, EditorContext>
  {
  public:
    static TextEditorNode *cast(Node *node)
    {
      return node && node->nodeTypeKey() == NodeTypeToken<TextEditorNode>()
                 ? static_cast<TextEditorNode *>(node) : 0;
    }
    static EditorContext *create(TextEditorNode *node, IPlatformController *, const LayoutState &)
    {
      return new EditorContext(node, seamKey());
    }
  };
}
