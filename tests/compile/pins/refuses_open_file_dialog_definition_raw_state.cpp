#include "app/OpenFileDialog.hpp"

void probe(loka::core::MutableState<loka::app::FileChooserResult> *state)
{
  loka::app::OpenFileDialog().result(state);
}
