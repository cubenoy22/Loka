#include "app/OpenFileDialog.hpp"

void probe(loka::core::MutableState<loka::app::FileChooserResult> *state)
{
  loka::app::SaveFileDialog(loka::core::String::Literal("Untitled"))
      .filterPolicy(loka::app::FILE_DIALOG_FILTER_ALL_FILES_TEXT).result(state);
}
