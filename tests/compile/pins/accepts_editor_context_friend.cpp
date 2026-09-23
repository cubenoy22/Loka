#include "editor_seam_key.hpp"
using namespace seam_key_pin;
class NullTextEditorContext : public NodeContext
{
public:
  void probe(TextEditorNode &node, const SeamKey<TextEditorNode> &key)
  {
    (void)key;
    (void)node.seam(key);
  }
};
