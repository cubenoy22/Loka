#include "app/FocusFact.hpp"
enum Field { HEIGHT };
namespace loka { namespace app {
template <> struct FocusKeyTraits<Field> : UnsignedFocusKeyTraits<Field> {};
} }
void pin()
{
  loka::app::FocusFact<Field> focus;
  focus.post(HEIGHT);
}
