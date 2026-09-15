#ifndef SMIRKYCARD_CARD_NODES_HPP
#define SMIRKYCARD_CARD_NODES_HPP

#include "ScriptRuntime.hpp"
#include "app/core/Window.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/scene/state/NodeState.hpp"
#include "core/State.hpp"

namespace smirkycard
{
  class CardScene : public loka::app::scene::Scene
  {
  public:
    explicit CardScene(loka::app::scene::NodeDefinitionBase *root)
        : Scene(root)
    {
    }
    void replaceWith(CardScene *next);
  };

  class JsCardNode;
  struct JsCardProps : loka::app::scene::NodePropsBase<JsCardProps>
  {
    typedef JsCardProps TypeTag;
    typedef JsCardNode NodeType;
    ScriptRuntime *runtime;
    SmirkyCardId card;
    JsCardProps(ScriptRuntime *value = 0, SmirkyCardId id = SMIRKY_CARD_FIRST)
        : runtime(value),
          card(id)
    {
    }
    bool operator<(const loka::app::scene::PropsBase &rhs) const;
  };

  /** The sole JS card boundary; it owns all duplicated JS values. */
  class JsCardNode : public loka::app::scene::StdCompositionBoundaryNodeBase<JsCardProps>
  {
  public:
    explicit JsCardNode(const JsCardProps &props);
    virtual ~JsCardNode();
    bool constructing() const;
    JSValue mintState(JSContext *context, JSValueConst initial);
    JSValue seatGet(JSContext *context, JSValueConst seat);
    JSValue seatSet(JSContext *context, JSValueConst seat, JSValueConst value);
    JSValue errorSeatGet(JSContext *context);
    void requestGo(const char *name, size_t length);
    void requestGo(SmirkyCardId card);
    void requestReload();
    bool setComposeTree(JSContext *context, JSValueConst tree);
    loka::app::scene::NodeDefinitionBase *lowerText(JSContext *context, JSValueConst tree);
    loka::app::scene::NodeDefinitionBase *lowerEditText(JSContext *context, JSValueConst tree);
    loka::app::scene::NodeDefinitionBase *lowerButton(JSContext *context, JSValueConst tree);
    void fail(const char *message);
    loka::app::scene::NodeDefinitionBase *lowerChild(JSContext *context, JSValueConst tree, int depth);
    virtual void declareBindings(loka::app::scene::BindingToken &token);
    virtual void composeNode(loka::app::scene::NodeComposition &composition);
    virtual void attachNode(loka::app::scene::NodeComposition &composition);
    virtual void detachNode(loka::app::scene::NodeComposition &composition);

  private:
    friend class IJsNodeLowering;
    void fail(const loka::core::String &message);
    loka::app::scene::NodeDefinitionBase *lower(JSContext *context, JSValueConst tree, int depth);
    int handlerSlot(JSContext *context, JSValueConst handler);
    void fire(int slot);
    void callHook(JSValueConst hook);
    void fire0();
    void fire1();
    void fire2();
    void fire3();
    void fire4();
    void fire5();
    void fire6();
    void fire7();
    bool constructing_;
    bool failed_;
    JSValue instance_;
    loka::core::String failure_;
    loka::core::EmitterState emitters_[8];
    int usedStates_;
    JSValue seats_[8], handlers_[8], errorSeat_, tree_, onAttach_, onDetach_;
    int seatKinds_[8];
    loka::app::scene::NodeState<loka::core::String> strings_[8], script_, result_, error_;
    loka::app::scene::NodeState<int> ints_[8];
    loka::app::scene::NodeState<bool> bools_[8];
    loka::app::scene::DerivedNodeState<loka::core::String> derivedStrings_[8];
  };
  CardScene *CreateCard(SmirkyCardId card, ScriptRuntime &runtime);
} // namespace smirkycard
#endif
