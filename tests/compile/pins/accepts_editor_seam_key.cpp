#include "editor_seam_key.hpp"
using namespace seam_key_pin;
NodeContext *probe(TextEditorNode &node, IPlatformController &controller, const LayoutState &state)
{
  EditorHandler handler;
  return handler.ensureContext(&node, &controller, state);
}
