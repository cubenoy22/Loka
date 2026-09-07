/**
 * UI bindings can be declared only through the kernel-issued BindingToken.
 * This refuses twin pins that contract together with
 * accepts_bindings_need_the_token.cpp in the same header environment.
 */
#include "app/scene/node/ComponentNode.hpp"

using namespace loka::app::scene;
class Probe : public ComponentNode
{
public:
  void probe()
  {
    this->bindActionForUi(emitter, this, &Probe::m);
  }
  virtual void composeChildren(NodeComposition &c) { (void)c; }
private:
  void m() {}
  loka::core::EmitterState emitter;
};
