#include "editor_seam_key.hpp"
using namespace seam_key_pin;
class ButtonKeySource : public RetainedNodeHandler<ButtonKeySource, ButtonNode, EditorContext>
{
public:
  static SeamKey<ButtonNode> probe()
  {
    return seamKey();
  }
};
