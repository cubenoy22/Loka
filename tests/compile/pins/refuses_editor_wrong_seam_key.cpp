#include "editor_seam_key.hpp"
using namespace seam_key_pin;
class ButtonContext : public NodeContext
{
public:
  void probe(TextEditorNode &node, const SeamKey<ButtonNode> &key)
  {
    (void)node.seam(key);
  }
};
