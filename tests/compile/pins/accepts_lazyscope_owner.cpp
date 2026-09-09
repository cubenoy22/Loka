#include "app/scene/boundary/LazyScopeDefinition.hpp"
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
class Probe : public LazyScopeNode
{
public:
  typedef ::Props Props;
  typedef Tag TypeTag;
  explicit Probe(const Props &p)
      : props(p)
  {
  }
  virtual void declareScope(NodeComposition &) {}
  Props props;
};
void acceptsLazyScope(loka::core::State<int> &key, NodeComposition &c)
{
  c.declare(LazyScope(key, Props()));
}
