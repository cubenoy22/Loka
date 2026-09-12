#include "app/OpenFileDialog.hpp"

void probe(const loka::app::scene::NodeState<loka::app::FileChooserResult> &state)
{
  loka::app::OpenFileDialogProps().result(state);
  loka::app::OpenFileDialog().result(state);
}
