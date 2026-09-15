#include "app/nodes/controls/EditText.hpp"
#include "app/scene/state/WriteSeat.hpp"

void probe(loka::core::MutableState<loka::core::String> *state)
{
  loka::app::EditText(loka::app::scene::WriteSeat<loka::core::String>(state));
}
