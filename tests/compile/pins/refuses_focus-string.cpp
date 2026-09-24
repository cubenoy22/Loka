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
  loka::app::scene::Reported<loka::app::Focused<loka::core::String> > fact;
  loka::app::EditText().focusedAs(fact, loka::core::String("bad"));
  loka::app::TextEditor().focusedAs(fact, loka::core::String("bad"));
}
