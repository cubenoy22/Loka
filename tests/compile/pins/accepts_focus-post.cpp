#include "app/Focus.hpp"
enum Field { HEIGHT };
namespace loka { namespace app {
template <> struct FocusKeyTraits<Field> : UnsignedFocusKeyTraits<Field> {};
} }
void pin()
{
  loka::app::Focus<Field> focus;
  focus.post(HEIGHT);
}
