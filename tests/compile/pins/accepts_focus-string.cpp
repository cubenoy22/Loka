#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/controls/TextEditor.hpp"
#include "core/String.hpp"
struct UnregisteredKey
{
};
enum RegisteredFocusKey
{
  FIELD
};
namespace loka
{
  namespace app
  {
    template <> struct FocusKeyTraits<RegisteredFocusKey> : UnsignedFocusKeyTraits<RegisteredFocusKey>
    {
    };
  } // namespace app
} // namespace loka
void pin()
{
  loka::app::scene::Reported<loka::app::Focused<RegisteredFocusKey> > fact;
  loka::app::EditText().focusedAs(fact, FIELD);
  loka::app::TextEditor().focusedAs(fact, FIELD);
  loka::app::scene::Reported<loka::app::Focused<int> > ids;
  loka::app::EditText().focusedAs(ids, -7);
  loka::app::TextEditor().focusedAs(ids, 7);
  loka::app::FocusBinding empty, copy(empty);
  empty = copy;
}
