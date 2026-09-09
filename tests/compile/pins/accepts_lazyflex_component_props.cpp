#include "app/nodes/nestable/LazyFlex.hpp"
using namespace loka::app::scene;
class Probe;
struct Props : NodePropsBase<Props>
{
  typedef Props TypeTag;
  typedef Probe NodeType;
  bool operator<(const PropsBase &) const
  {
    return false;
  }
  bool operator!=(const Props &) const
  {
    return false;
  }
};
class Probe : public ComponentNodeWithProps<Props>
{
public:
  typedef ::Props TypeTag;
  explicit Probe(const Props &p)
      : ComponentNodeWithProps<Props>(p)
  {
    (void)p;
  }
  virtual void composeChildren(NodeComposition &) {}
};
void pin(loka::core::ObservableList<Props> &list, NodeComposition &c)
{
  c.declare(loka::app::LazyColumn(list));
}
