#ifndef SMIRKYCARD_CARD_NODES_HPP
#define SMIRKYCARD_CARD_NODES_HPP

#include "ScriptRuntime.hpp"
#include "app/core/Window.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/Text.hpp"
#include "core/util/OwnedDef.hpp"
#include <new>
#include <cstring>

namespace smirkycard
{
  /** Adopts navigation intent; the App clock prepares and projects the replacement
      through the existing Window before publishing its ON_ATTACH lifecycle event. */
  class CardScene : public loka::app::scene::Scene
  {
  public:
    explicit CardScene(loka::app::scene::NodeDefinitionBase *root)
        : loka::app::scene::Scene(root)
    {
    }

    void replaceWith(CardScene *next)
    {
      assert(next && this->getWindow());
      this->getWindow()->sceneManager()->commitTransaction(this, next);
    }
  };

  template <SmirkyCardId Card> class CardNode;

  /** Borrows the AppConfig's runtime, which outlives every Window and Scene. */
  template <SmirkyCardId Card> struct CardProps : loka::app::scene::NodePropsBase<CardProps<Card> >
  {
    typedef CardProps<Card> TypeTag;
    typedef CardNode<Card> NodeType;
    explicit CardProps(ScriptRuntime *value = 0)
        : runtime(value)
    {
    }
    bool operator<(const loka::app::scene::PropsBase &rhs) const
    {
      return rhs.propsTypeId() == this->propsTypeId() && this->runtime < static_cast<const CardProps &>(rhs).runtime;
    }
    ScriptRuntime *runtime;
  };

  inline CardScene *CreateCard(SmirkyCardId card, ScriptRuntime &runtime);

  /** Each card is a C++ boundary. JavaScript chooses the next Scene by name. */
  template <SmirkyCardId Card>
  class CardNode : public loka::app::scene::StdCompositionBoundaryNodeBase<CardProps<Card> >
  {
  public:
    explicit CardNode(const CardProps<Card> &p)
        : loka::app::scene::StdCompositionBoundaryNodeBase<CardProps<Card> >(p),
          run_(),
          error_()
    {
      assert(p.runtime);
      this->state(this->error_, loka::core::String::Literal("Ready"));
    }

    virtual void declareBindings(loka::app::scene::BindingToken &token)
    {
      token.action(this->run_, this, &CardNode::runScript);
    }

    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      using namespace loka::app;
      composition.declare(Box().padding(12)
                          << (Column()
                              << Text(Card == SMIRKY_CARD_FIRST ? "Card One" : "Card Two").TEST_ID("SmirkyCard.Title")
                              << Text("This Scene is defined in C++.")
                              << Text(loka::core::String::Literal("JS: ") + loka::core::String::Literal(Script()))
                              << Button("Run JavaScript", &this->run_).TEST_ID("SmirkyCard.Run")
                              << Text(this->error_.state()).TEST_ID("SmirkyCard.Status")));
    }

  private:
    static const char *Script()
    {
      return Card == SMIRKY_CARD_FIRST ? "['first', 'second'][1]" : "['first', 'second'][0]";
    }

    void runScript()
    {
      char error[256];
      ScriptRuntime &runtime = *this->props.runtime;
      const SmirkyCardId next = runtime.evaluate(Script(), error, sizeof(error));
      if (next == SMIRKY_CARD_ERROR)
      {
        this->error_.set(loka::core::String::Utf8(error, std::strlen(error)));
        return;
      }
      CardScene *scene = CreateCard(next, runtime);
      if (!scene)
      {
        this->error_.set(loka::core::String::Literal("Could not create the next card."));
        return;
      }
      // Evaluation and JS value cleanup have finished. The swap detaches this
      // boundary synchronously; do not access its members after the handoff.
      // CreateCard is the only materialization path for these card boundaries.
      static_cast<CardScene *>(this->scene())->replaceWith(scene);
    }

    loka::core::EmitterState run_;
    loka::app::scene::NodeState<loka::core::String> error_;
  };

  inline CardScene *CreateCard(SmirkyCardId card, ScriptRuntime &runtime)
  {
    loka::core::OwnedDef<loka::app::scene::NodeDefinitionBase> root;
    switch (card)
    {
    case SMIRKY_CARD_FIRST:
      root.reset(
          loka::app::scene::Boundary<CardNode<SMIRKY_CARD_FIRST> >(CardProps<SMIRKY_CARD_FIRST>(&runtime)).clone());
      break;
    case SMIRKY_CARD_SECOND:
      root.reset(
          loka::app::scene::Boundary<CardNode<SMIRKY_CARD_SECOND> >(CardProps<SMIRKY_CARD_SECOND>(&runtime)).clone());
      break;
    case SMIRKY_CARD_ERROR:
      return 0;
    }
    if (!root.isSet())
      return 0;
    CardScene *scene = new (std::nothrow) CardScene(root.get());
    if (scene)
      root.take();
    return scene;
  }
} // namespace smirkycard
#endif
