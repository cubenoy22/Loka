#include "app/scene/boundary/LazyScopeDefinition.hpp"
#include "app/scene/node/ComponentNode.hpp"
using namespace loka::app::scene;
class Probe;
struct Tag
{
};
struct Props : NodePropsBase<Props>
{
  typedef Tag TypeTag;
  typedef Probe NodeType;
  bool operator<(const PropsBase &) const
  {
    return false;
  }
};
class Probe : public ComponentNode
{
public:
  typedef ::Props Props;
  typedef Tag TypeTag;
  explicit Probe(const Props &p)
      : props(p)
  {
  }
  Props props;

protected:
  virtual void composeChildren(NodeComposition &) {}
};
void refusesComponentScope(loka::core::State<int> &key, NodeComposition &c)
{
  c.declare(LazyScope(key, Props()));
}
