#include "AppComponentGroupTests.hpp"
#include "app/core/AppComponentGroup.hpp"
#include "support/TestVerify.hpp"

namespace
{
  std::vector<AppComponent *> BootstrapComponents(AppComponent *first, AppComponent *second)
  {
    std::vector<AppComponent *> components;
    components.push_back(first);
    components.push_back(second);
    return components;
  }
} // namespace

void testAppComponentGroupValueRows()
{
  AppComponent first, second, keyed, unkeyed, foreign;
  AppComponentGroup group(BootstrapComponents(&first, &second));
  const AppComponentGroup::Components components = group.getComponents();
  LOKA_VERIFY(components.size() == 2);
  LOKA_VERIFY(components[0] == &first && components[1] == &second);
  LOKA_VERIFY(group.keyOf(&first).isNone());
  LOKA_VERIFY(group.keyOf(&second).isNone());
  LOKA_VERIFY(group.keyOf(&foreign).isNone());

  group.reserve(2);
  LOKA_VERIFY(components.size() == 2);
  const loka::core::ItemId id(7, 23);
  group.adopt(&keyed, id);
  group.adopt(&unkeyed);
  LOKA_VERIFY(components.size() == 4);
  LOKA_VERIFY(components[2] == &keyed && components[3] == &unkeyed);
  LOKA_VERIFY(group.keyOf(&keyed) == id);
  LOKA_VERIFY(group.find(id) == &keyed);
  LOKA_VERIFY(!group.find(loka::core::ItemId::none()));
  LOKA_VERIFY(!group.find(loka::core::ItemId(7, 24)));
  LOKA_VERIFY(group.keyOf(&unkeyed).isNone());

  LOKA_VERIFY(group.remove(&keyed));
  LOKA_VERIFY(group.keyOf(&keyed).isNone());
  LOKA_VERIFY(!group.find(id));
  LOKA_VERIFY(components.size() == 3);
  LOKA_VERIFY(components[0] == &first && components[1] == &second && components[2] == &unkeyed);
  LOKA_VERIFY(!group.remove(&foreign));
  LOKA_VERIFY(components.size() == 3);

  // Release the borrowed stack components before the group's owning destructor.
  const std::vector<AppComponent *> released = group.build();
  LOKA_VERIFY(released.size() == 3);
  LOKA_VERIFY(released[0] == &first && released[1] == &second && released[2] == &unkeyed);
  LOKA_VERIFY(components.size() == 0);
  LOKA_VERIFY(group.keyOf(&first).isNone());
}
