#include "CardNodes.hpp"
#include "JsOwnProperties.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/Text.hpp"
#include "core/util/OwnedDef.hpp"
#include <new>
#include <string>

namespace smirkycard
{
  namespace
  {
    JSClassID seatClassId = 0;
    int ensureSeatClass(JSRuntime *runtime)
    {
      JSClassDef def;
      memset(&def, 0, sizeof(def));
      def.class_name = "SmirkyCardSeat";
      if (!seatClassId)
        JS_NewClassID(runtime, &seatClassId);
      // Class IDs are process-global, while class definitions are per runtime.
      // QuickJS returns -1 when this runtime already has this ID.
      const int registered = JS_NewClass(runtime, seatClassId, &def);
      return registered == 0 || registered == -1;
    }
    JSValue seatGetNative(JSContext *ctx, JSValueConst thisValue, int, JSValueConst *)
    {
      JsCardNode *node = static_cast<JsCardNode *>(JS_GetOpaque2(ctx, thisValue, seatClassId));
      return node ? node->seatGet(ctx, thisValue) : JS_UNDEFINED;
    }
    JSValue seatSetNative(JSContext *ctx, JSValueConst thisValue, int argc, JSValueConst *argv)
    {
      JsCardNode *node = static_cast<JsCardNode *>(JS_GetOpaque2(ctx, thisValue, seatClassId));
      if (!node)
        return JS_ThrowTypeError(ctx, "seat belongs to a retired card");
      if (argc != 1)
        return JS_ThrowTypeError(ctx, "seat.set(value) requires one value");
      return node->seatSet(ctx, thisValue, argv[0]);
    }
    JSValue errorGetNative(JSContext *ctx, JSValueConst thisValue, int, JSValueConst *)
    {
      JsCardNode *node = static_cast<JsCardNode *>(JS_GetOpaque2(ctx, thisValue, seatClassId));
      return node ? node->errorSeatGet(ctx) : JS_UNDEFINED;
    }
    JSValue jsString(JSContext *ctx, const loka::core::String &value)
    {
      const loka::core::StringBuffer buffer = value.bufferWithEncoding(loka::core::StringEncodingUtf8);
      return JS_NewStringLen(ctx, static_cast<const char *>(buffer.data()), buffer.length());
    }
    bool integerNumber(JSContext *ctx, JSValueConst value, int32_t &out)
    {
      double number = 0;
      if (!JS_IsNumber(value) || JS_ToFloat64(ctx, &number, value) || number != number || number < -2147483648.0
          || number > 2147483647.0 || static_cast<double>(static_cast<int32_t>(number)) != number)
        return false;
      out = static_cast<int32_t>(number);
      return true;
    }
    class FormatIntEval : public loka::core::DerivedState<loka::core::String>::EvalFn
    {
    public:
      explicit FormatIntEval(loka::app::scene::NodeState<int> *value)
          : value_(value)
      {
      }
      virtual loka::core::String operator()()
      {
        return loka::core::String::FromInt(value_->get());
      }

    private:
      loka::app::scene::NodeState<int> *value_;
    };
    class FormatBoolEval : public loka::core::DerivedState<loka::core::String>::EvalFn
    {
    public:
      explicit FormatBoolEval(loka::app::scene::NodeState<bool> *value)
          : value_(value)
      {
      }
      virtual loka::core::String operator()()
      {
        return loka::core::String::Literal(value_->get() ? "true" : "false");
      }

    private:
      loka::app::scene::NodeState<bool> *value_;
    };
  } // namespace
  void CardScene::replaceWith(CardScene *next)
  {
    assert(next && this->getWindow());
    this->getWindow()->sceneManager()->commitTransaction(this, next);
  }
  bool JsCardProps::operator<(const loka::app::scene::PropsBase &rhs) const
  {
    const JsCardProps &o = static_cast<const JsCardProps &>(rhs);
    return runtime != o.runtime ? runtime < o.runtime : card < o.card;
  }
  JsCardNode::JsCardNode(const JsCardProps &p)
      : loka::app::scene::StdCompositionBoundaryNodeBase<JsCardProps>(p),
        engine_(p.runtime ? p.runtime->currentEngine() : 0),
        engineRef_(this->engine_),
        constructing_(false),
        failed_(false),
        instance_(JS_UNDEFINED),
        usedStates_(0),
        errorSeat_(JS_UNDEFINED),
        tree_(JS_UNDEFINED),
        onAttach_(JS_UNDEFINED),
        onDetach_(JS_UNDEFINED)
  {
    for (int i = 0; i < 8; ++i)
    {
      seats_[i] = JS_UNDEFINED;
      handlers_[i] = JS_UNDEFINED;
      seatKinds_[i] = -1;
    }
    this->state(error_, loka::core::String::Literal(""));
    JSContext *ctx = this->engine_ ? this->engine_->context() : 0;
    JSValue ctor = ctx ? this->engine_->constructorFor(p.card) : JS_UNDEFINED;
    loka::core::String error;
    if (!ctx || JS_IsUndefined(ctor))
    {
      fail("Card is not registered.");
      return;
    }
    ScriptRuntime::InterruptWindow interrupt(*p.runtime, *this->engine_);
    p.runtime->setActive(this);
    constructing_ = true;
    bool ok = p.runtime->callConstructor(*this->engine_, ctor, instance_, error);
    constructing_ = false;
    p.runtime->setActive(0);
    JS_FreeValue(ctx, ctor);
    if (!ok)
      fail("Card constructor failed.");
    if (ok)
    {
      JSValue attach = JS_GetPropertyStr(ctx, instance_, "onAttach");
      JSValue detach = JS_GetPropertyStr(ctx, instance_, "onDetach");
      if (JS_IsFunction(ctx, attach))
        onAttach_ = JS_DupValue(ctx, attach);
      if (JS_IsFunction(ctx, detach))
        onDetach_ = JS_DupValue(ctx, detach);
      JS_FreeValue(ctx, attach);
      JS_FreeValue(ctx, detach);
    }
    if (ok && ensureSeatClass(this->engine_->jsRuntime()))
    {
      errorSeat_ = JS_NewObjectClass(ctx, seatClassId);
      JS_SetOpaque(errorSeat_, this);
      JS_SetPropertyStr(ctx, errorSeat_, "get", JS_NewCFunction(ctx, errorGetNative, "get", 0));
      JS_FreezeObject(ctx, errorSeat_);
      JS_SetPropertyStr(ctx, instance_, "error", JS_DupValue(ctx, errorSeat_));
    }
  }
  JsCardNode::~JsCardNode()
  {
    if (this->engine_)
    {
      JSContext *ctx = this->engine_->context();
      if (JS_IsObject(errorSeat_))
        JS_SetOpaque(errorSeat_, 0);
      for (int i = 0; i < 8; ++i)
        if (JS_IsObject(seats_[i]))
          JS_SetOpaque(seats_[i], 0);
      JS_FreeValue(ctx, instance_);
      JS_FreeValue(ctx, tree_);
      JS_FreeValue(ctx, onAttach_);
      JS_FreeValue(ctx, onDetach_);
      JS_FreeValue(ctx, errorSeat_);
      for (int i = 0; i < 8; ++i)
      {
        JS_FreeValue(ctx, seats_[i]);
        JS_FreeValue(ctx, handlers_[i]);
      }
    }
  }
  bool JsCardNode::constructing() const
  {
    return constructing_;
  }
  JSValue JsCardNode::mintState(JSContext *ctx, JSValueConst initial)
  {
    if (!constructing_ || usedStates_ >= 8)
    {
      fail("state() is only valid in a constructor (maximum 8 seats).");
      return JS_UNDEFINED;
    }
    if (JS_IsString(initial))
    {
      size_t length = 0;
      const char *text = JS_ToCStringLen(ctx, &length, initial);
      if (!text)
      {
        fail("state() initial value is unsupported.");
        return JS_UNDEFINED;
      }
      this->state(strings_[usedStates_], loka::core::String::Utf8(text, length));
      JS_FreeCString(ctx, text);
    }
    else if (JS_IsNumber(initial))
    {
      int32_t value = 0;
      if (!integerNumber(ctx, initial, value))
      {
        fail("state(number) requires an integer");
        return JS_UNDEFINED;
      }
      this->state(ints_[usedStates_], static_cast<int>(value));
      this->derived(
          derivedStrings_[usedStates_], ints_[usedStates_], new (std::nothrow) FormatIntEval(&ints_[usedStates_]));
    }
    else if (JS_IsBool(initial))
    {
      this->state(bools_[usedStates_], JS_ToBool(ctx, initial) != 0);
      this->derived(
          derivedStrings_[usedStates_], bools_[usedStates_], new (std::nothrow) FormatBoolEval(&bools_[usedStates_]));
    }
    else
    {
      fail("state() initial value is unsupported.");
      return JS_UNDEFINED;
    }
    if (!ensureSeatClass(this->engine_->jsRuntime()))
    {
      fail("Could not create state seat.");
      return JS_UNDEFINED;
    }
    JSValue seat = JS_NewObjectClass(ctx, seatClassId);
    JS_SetOpaque(seat, this);
    JS_SetPropertyStr(ctx, seat, "get", JS_NewCFunction(ctx, seatGetNative, "get", 0));
    JS_SetPropertyStr(ctx, seat, "set", JS_NewCFunction(ctx, seatSetNative, "set", 1));
    seatKinds_[usedStates_] = JS_IsString(initial) ? 0 : JS_IsNumber(initial) ? 1 : 2;
    seats_[usedStates_++] = JS_DupValue(ctx, seat);
    JS_FreezeObject(ctx, seat);
    return seat;
  }
  JSValue JsCardNode::seatGet(JSContext *ctx, JSValueConst seat)
  {
    for (int i = 0; i < usedStates_; ++i)
      if (JS_IsStrictEqual(ctx, seat, seats_[i]))
      {
        if (seatKinds_[i] == 0)
          return jsString(ctx, strings_[i].get());
        if (seatKinds_[i] == 1)
          return JS_NewInt32(ctx, ints_[i].get());
        return JS_NewBool(ctx, bools_[i].get());
      }
    return JS_ThrowTypeError(ctx, "unknown state seat");
  }
  JSValue JsCardNode::seatSet(JSContext *ctx, JSValueConst seat, JSValueConst value)
  {
    for (int i = 0; i < usedStates_; ++i)
      if (JS_IsStrictEqual(ctx, seat, seats_[i]))
      {
        if (seatKinds_[i] == 0 && JS_IsString(value))
        {
          size_t n = 0;
          const char *s = JS_ToCStringLen(ctx, &n, value);
          if (!s)
            return JS_EXCEPTION;
          strings_[i].writeSeat().set(loka::core::String::Utf8(s, n), true);
          JS_FreeCString(ctx, s);
          return JS_UNDEFINED;
        }
        if (seatKinds_[i] == 1 && JS_IsNumber(value))
        {
          int32_t v = 0;
          if (!integerNumber(ctx, value, v))
          {
            error_.set(loka::core::String::Literal("state(number) requires an integer"));
            return JS_UNDEFINED;
          }
          ints_[i].writeSeat().set(static_cast<int>(v), true);
          return JS_UNDEFINED;
        }
        if (seatKinds_[i] == 2 && JS_IsBool(value))
        {
          bools_[i].writeSeat().set(JS_ToBool(ctx, value) != 0, true);
          return JS_UNDEFINED;
        }
        fail("state seat type mismatch.");
        return JS_UNDEFINED;
      }
    return JS_ThrowTypeError(ctx, "unknown state seat");
  }
  JSValue JsCardNode::errorSeatGet(JSContext *ctx)
  {
    return jsString(ctx, error_.get());
  }
  void JsCardNode::requestGo(const char *name, size_t length)
  {
    SmirkyCardId card = length == 5 && !memcmp(name, "first", 5)    ? SMIRKY_CARD_FIRST
                        : length == 6 && !memcmp(name, "second", 6) ? SMIRKY_CARD_SECOND
                                                                    : SMIRKY_CARD_ERROR;
    if (card == SMIRKY_CARD_ERROR)
    {
      fail("go() requires first or second.");
      return;
    }
    requestGo(card);
  }
  void JsCardNode::requestGo(SmirkyCardId card)
  {
    CardScene *scene = static_cast<CardScene *>(this->scene());
    if (!scene)
    {
      // The detach line clears the scene before detachNode runs, so go()
      // from onDetach has nowhere to hand the next card.
      fail("go() is unavailable while the card is detaching.");
      return;
    }
    CardScene *next = CreateCard(card, *props.runtime);
    if (next)
      scene->replaceWith(next);
    else
      fail("Could not create the next card.");
  }
  void JsCardNode::requestReload()
  {
    CardScene *scene = static_cast<CardScene *>(this->scene());
    if (!scene)
    {
      fail("reload() is unavailable while the card is detaching.");
      return;
    }
    loka::core::String error;
    JsEngine *candidate = props.runtime->prepareReload(props.card, error);
    if (!candidate)
    {
      fail(error);
      return;
    }
    // Build the replacement Scene first; the engine swap and the visible card
    // then change together, or neither does.
    CardScene *next = CreateCard(props.card, *props.runtime);
    if (!next)
    {
      props.runtime->discardReload(candidate);
      fail("Could not create the reloaded card.");
      return;
    }
    props.runtime->commitReload(candidate);
    scene->replaceWith(next);
  }
  void JsCardNode::declareBindings(loka::app::scene::BindingToken &t)
  {
    t.action(emitters_[0], this, &JsCardNode::fire0);
    t.action(emitters_[1], this, &JsCardNode::fire1);
    t.action(emitters_[2], this, &JsCardNode::fire2);
    t.action(emitters_[3], this, &JsCardNode::fire3);
    t.action(emitters_[4], this, &JsCardNode::fire4);
    t.action(emitters_[5], this, &JsCardNode::fire5);
    t.action(emitters_[6], this, &JsCardNode::fire6);
    t.action(emitters_[7], this, &JsCardNode::fire7);
    t.action(reloadEmitter_, this, &JsCardNode::requestReload);
  }
  // The card hooks ride the root boundary's own attach/detach doors
  // (ComponentNode::composeWithContext -> attachNode/detachNode), which every
  // path reaches: first mount, Scene::updateAttached, unmount and the
  // SceneManager switch. The lifecycle fact is not that door: a root starts
  // ATTACHED and teardown marks it RETIRED directly.
  void JsCardNode::callHook(JSValueConst hook)
  {
    if (!JS_IsFunction(this->engine_->context(), hook))
      return;
    loka::core::String error;
    JSValue result = JS_UNDEFINED;
    props.runtime->setActive(this);
    if (!props.runtime->call(*this->engine_, hook, instance_, 0, 0, result, error))
      error_.set(error);
    props.runtime->setActive(0);
    JS_FreeValue(this->engine_->context(), result);
  }
  void JsCardNode::attachNode(loka::app::scene::NodeComposition &composition)
  {
    StdCompositionBoundaryNodeBase<JsCardProps>::attachNode(composition);
    callHook(onAttach_);
  }
  void JsCardNode::detachNode(loka::app::scene::NodeComposition &composition)
  {
    // Before the base drops the owner slots, so the hook can still write seats.
    callHook(onDetach_);
    StdCompositionBoundaryNodeBase<JsCardProps>::detachNode(composition);
  }
  void JsCardNode::composeNode(loka::app::scene::NodeComposition &c)
  {
    using namespace loka::app;
    // A MAIN.JS startup failure is the runtime's fact; the card only shows it.
    {
      const loka::core::String mainError = props.runtime->mainErrorFor(props.card);
      if (!mainError.empty())
        error_.set(mainError);
    }
    if (!failed_ && JS_IsUndefined(tree_))
    {
      JSContext *ctx = this->engine_->context();
      ScriptRuntime::InterruptWindow interrupt(*props.runtime, *this->engine_);
      JSValue compose = JS_GetPropertyStr(ctx, instance_, "compose");
      JSValue result = JS_UNDEFINED;
      loka::core::String error;
      JSValue delegate = JS_NewObject(ctx);
      JS_SetPropertyStr(ctx, delegate, "declare", JS_NewCFunction(ctx, &ScriptRuntime::declare, "declare", 1));
      JS_FreezeObject(ctx, delegate);
      props.runtime->setActive(this);
      if (JS_IsException(compose))
      {
        props.runtime->captureException(*this->engine_, error);
        fail(error);
      }
      else if (!JS_IsFunction(ctx, compose)
               || !props.runtime->call(*this->engine_, compose, instance_, 1, &delegate, result, error))
        fail(error.empty() ? loka::core::String::Literal("JavaScript compose() failed.") : error);
      else if (JS_IsObject(result))
        setComposeTree(ctx, result);
      else if (JS_IsUndefined(tree_))
        fail("JavaScript compose() must return or declare a tree.");
      props.runtime->setActive(0);
      JS_FreeValue(ctx, delegate);
      JS_FreeValue(ctx, result);
      JS_FreeValue(ctx, compose);
    }
    if (failed_)
    {
      error_.set(failure_);
      this->declareRefusal(c);
      return;
    }
    ScriptRuntime::InterruptWindow interrupt(*props.runtime, *this->engine_);
    loka::core::OwnedDef<loka::app::scene::NodeDefinitionBase> definition(lower(this->engine_->context(), tree_, 0));
    if (!definition.isSet())
    {
      this->declareRefusal(c);
      return;
    }
    c.declare(*definition.get());
  }
  void JsCardNode::fail(const char *message)
  {
    fail(loka::core::String::Literal(message));
  }
  void JsCardNode::declareRefusal(loka::app::scene::NodeComposition &c)
  {
    using namespace loka::app;
    c.declare(VStack() << Text(error_.state()).TEST_ID("SmirkyCard.Status")
                       << Button("Reload MAIN.JS", &reloadEmitter_).TEST_ID("SmirkyCard.Reload"));
  }
  void JsCardNode::fail(const loka::core::String &message)
  {
    failed_ = true;
    failure_ = message;
    if (error_.isValid())
      error_.set(failure_);
  }
  bool JsCardNode::setComposeTree(JSContext *ctx, JSValueConst value)
  {
    if (!JS_IsObject(value) || JS_IsArray(value))
    {
      fail("JavaScript compose tree must be an object.");
      return false;
    }
    if (!JS_IsUndefined(tree_))
    {
      fail("JavaScript compose() declared more than one tree.");
      return false;
    }
    tree_ = JS_DupValue(ctx, value);
    return true;
  }
  namespace
  {
    bool treeString(JSContext *ctx, JSValueConst tree, const char *name, loka::core::String &out)
    {
      JSValue value = JS_GetPropertyStr(ctx, tree, name);
      size_t length = 0;
      const char *text = JS_IsString(value) ? JS_ToCStringLen(ctx, &length, value) : 0;
      if (text)
      {
        out = loka::core::String::Utf8(text, length);
        JS_FreeCString(ctx, text);
      }
      JS_FreeValue(ctx, value);
      return text != 0;
    }
  } // namespace
  loka::app::scene::NodeDefinitionBase *JsCardNode::lower(JSContext *ctx, JSValueConst tree, int depth)
  {
    using namespace loka::app;
    if (depth > 8 || !JS_IsObject(tree) || JS_IsArray(tree))
    {
      fail(depth > 8 ? "JavaScript tree exceeds depth 8." : "JavaScript tree has an invalid node.");
      return 0;
    }
    JSValue kindValue = JS_GetPropertyStr(ctx, tree, "kind");
    int32_t kind = 0;
    int converted = JS_ToInt32(ctx, &kind, kindValue);
    JS_FreeValue(ctx, kindValue);
    if (converted)
    {
      fail("JavaScript tree node has an unknown kind.");
      return 0;
    }
    IJsNodeLowering *lowering = props.runtime->registry_.lookup(kind);
    if (!lowering)
    {
      fail("JavaScript tree node has an unknown kind.");
      return 0;
    }
    loka::app::scene::NodeDefinitionBase *out = lowering->lower(*this, ctx, tree, depth);
    if (!out)
    {
      if (!failed_)
        fail("Could not allocate JavaScript tree.");
      return 0;
    }
    loka::core::String id;
    if (treeString(ctx, tree, "testId", id))
    {
      const loka::core::StringBuffer b = id.bufferWithEncoding(loka::core::StringEncodingUtf8);
      const std::string utf8(static_cast<const char *>(b.data()), b.length());
      out->setTestId(utf8.c_str());
    }
    return out;
  }
  namespace
  {
    bool requirePlainStyleObject(JSContext *ctx, JSValueConst dict, const char *label)
    {
      // Compare to an ordinary object without consulting replaceable JS globals.
      JSValue plain = JS_NewObject(ctx);
      if (JS_IsException(plain))
        return false;
      JSValue prototype = JS_IsObject(dict) ? JS_GetPrototype(ctx, dict) : JS_UNDEFINED;
      JSValue plainPrototype = JS_GetPrototype(ctx, plain);
      const bool valid = JS_IsObject(dict) && JS_GetClassID(dict) == JS_GetClassID(plain)
                         && (JS_IsNull(prototype) || JS_IsStrictEqual(ctx, prototype, plainPrototype));
      const bool exception = JS_IsException(prototype) || JS_IsException(plainPrototype);
      JS_FreeValue(ctx, plainPrototype);
      JS_FreeValue(ctx, prototype);
      JS_FreeValue(ctx, plain);
      if (exception)
        return false;
      if (!valid)
      {
        JS_ThrowTypeError(ctx, "%s requires a plain object", label);
        return false;
      }
      return true;
    }
  } // namespace

  bool readTextStyle(JSContext *ctx, JSValueConst dict, loka::app::TextStyle &out)
  {
    using namespace loka::app;
    if (!requirePlainStyleObject(ctx, dict, "Text style"))
      return false;
    JsOwnProperties names(ctx);
    if (!names.read(dict, JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK))
      return false;
    TextStyle result;
    for (uint32_t i = 0; i < names.count(); ++i)
    {
      JSValue key = JS_AtomToValue(ctx, names.atom(i));
      size_t length = 0;
      const char *name = JS_IsString(key) ? JS_ToCStringLen(ctx, &length, key) : 0;
      JS_FreeValue(ctx, key);
      if (!name)
      {
        JS_ThrowTypeError(ctx, "Unknown Text style key");
        return false;
      }
      const bool size = length == 4 && !memcmp(name, "size", 4);
      const bool weight = length == 6 && !memcmp(name, "weight", 6);
      const bool italic = length == 6 && !memcmp(name, "italic", 6);
      JS_FreeCString(ctx, name);
      if (!size && !weight && !italic)
      {
        JS_ThrowTypeError(ctx, "Unknown Text style key");
        return false;
      }
      JSValue value = JS_GetProperty(ctx, dict, names.atom(i));
      if (JS_IsException(value))
        return false;
      bool accepted = false;
      if (size)
      {
        int32_t number = 0;
        accepted = integerNumber(ctx, value, number);
        if (accepted)
          result = result + SizeOf(number);
      }
      else if (italic)
      {
        accepted = JS_IsBool(value);
        if (accepted)
          result.italic(JS_ToBool(ctx, value) != 0);
      }
      else if (JS_IsString(value))
      {
        // StyleVocab.hpp supplies enums but no name-to-enum table (#939).
        const struct WeightName
        {
          const char *name;
          TextWeight value;
        } weights[] = {{"normal", TEXT_WEIGHT_NORMAL}, {"bold", TEXT_WEIGHT_BOLD}};
        size_t weightLength = 0;
        const char *weightName = JS_ToCStringLen(ctx, &weightLength, value);
        if (!weightName)
        {
          JS_FreeValue(ctx, value);
          return false;
        }
        for (size_t j = 0; j < sizeof(weights) / sizeof(weights[0]); ++j)
          if (weightLength == strlen(weights[j].name) && !memcmp(weightName, weights[j].name, weightLength))
          {
            result.weight(weights[j].value);
            accepted = true;
            break;
          }
        JS_FreeCString(ctx, weightName);
      }
      JS_FreeValue(ctx, value);
      if (!accepted)
      {
        JS_ThrowTypeError(ctx, "Invalid Text style value");
        return false;
      }
    }
    out = result;
    return true;
  }
  bool readBlockStyle(JSContext *ctx, JSValueConst dict, loka::app::BlockStyle &out)
  {
    using namespace loka::app;
    if (!requirePlainStyleObject(ctx, dict, "Block style"))
      return false;
    JsOwnProperties names(ctx);
    if (!names.read(dict, JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK))
      return false;
    BlockStyle result;
    // StyleVocab.hpp supplies enums but no name-to-enum table (#951, like weights).
    const struct BlockName
    {
      const char *key;
      const char *name;
      BlockStyle style;
    } values[] = {{"align", "left", BlockStyle().align(TEXT_ALIGN_LEFT)},
                  {"align", "center", BlockStyle().align(TEXT_ALIGN_CENTER)},
                  {"align", "right", BlockStyle().align(TEXT_ALIGN_RIGHT)},
                  {"wrap", "none", BlockStyle().wrap(TEXT_WRAP_NONE)},
                  {"wrap", "word", BlockStyle().wrap(TEXT_WRAP_WORD)},
                  {"wrap", "char", BlockStyle().wrap(TEXT_WRAP_CHAR)},
                  {"truncation", "none", BlockStyle().truncation(TEXT_TRUNCATION_NONE)},
                  {"truncation", "clip", BlockStyle().truncation(TEXT_TRUNCATION_CLIP)},
                  {"truncation", "ellipsis", BlockStyle().truncation(TEXT_TRUNCATION_ELLIPSIS)}};
    for (uint32_t i = 0; i < names.count(); ++i)
    {
      JSValue key = JS_AtomToValue(ctx, names.atom(i));
      size_t keyLength = 0;
      const char *keyName = JS_IsString(key) ? JS_ToCStringLen(ctx, &keyLength, key) : 0;
      JS_FreeValue(ctx, key);
      bool known = false;
      for (size_t j = 0; keyName && j < sizeof(values) / sizeof(values[0]); ++j)
        if (keyLength == strlen(values[j].key) && !memcmp(keyName, values[j].key, keyLength))
          known = true;
      if (!known)
      {
        JS_FreeCString(ctx, keyName);
        JS_ThrowTypeError(ctx, "Unknown Block style key");
        return false;
      }
      JSValue value = JS_GetProperty(ctx, dict, names.atom(i));
      if (JS_IsException(value))
      {
        JS_FreeCString(ctx, keyName);
        return false;
      }
      size_t length = 0;
      const char *name = JS_IsString(value) ? JS_ToCStringLen(ctx, &length, value) : 0;
      bool accepted = false;
      for (size_t j = 0; name && j < sizeof(values) / sizeof(values[0]); ++j)
        if (keyLength == strlen(values[j].key) && !memcmp(keyName, values[j].key, keyLength)
            && length == strlen(values[j].name) && !memcmp(name, values[j].name, length))
        {
          result = result + values[j].style;
          accepted = true;
          break;
        }
      JS_FreeCString(ctx, name);
      JS_FreeCString(ctx, keyName);
      JS_FreeValue(ctx, value);
      if (!accepted)
      {
        JS_ThrowTypeError(ctx, "Invalid Block style value");
        return false;
      }
    }
    out = result;
    return true;
  }
  loka::app::scene::NodeDefinitionBase *JsCardNode::lowerText(JSContext *ctx, JSValueConst tree)
  {
    using namespace loka::app;
    TextStyle style;
    JSValue dict = JS_GetPropertyStr(ctx, tree, "style");
    bool valid = !JS_IsException(dict) && (JS_IsUndefined(dict) || readTextStyle(ctx, dict, style));
    JS_FreeValue(ctx, dict);
    BlockStyle block;
    if (valid)
    {
      dict = JS_GetPropertyStr(ctx, tree, "block");
      valid = !JS_IsException(dict) && (JS_IsUndefined(dict) || readBlockStyle(ctx, dict, block));
      JS_FreeValue(ctx, dict);
    }
    if (!valid)
    {
      loka::core::String error;
      props.runtime->captureException(*this->engine_, error);
      fail(error);
      return 0;
    }
    JSValue value = JS_GetPropertyStr(ctx, tree, "text");
    int seat = -1;
    for (int i = 0; i < usedStates_; ++i)
      if (JS_IsStrictEqual(ctx, value, seats_[i]))
      {
        seat = i;
        break;
      }
    loka::app::scene::NodeDefinitionBase *out =
        JS_IsStrictEqual(ctx, value, errorSeat_)
            ? static_cast<loka::app::scene::NodeDefinitionBase *>(
                  new (std::nothrow) TextDefinitionWithAttr(Text(error_.state()) + style + block))
        : seat >= 0
            ? (seatKinds_[seat] == 0
                   ? static_cast<loka::app::scene::NodeDefinitionBase *>(
                         new (std::nothrow) TextDefinitionWithAttr(Text(strings_[seat].state()) + style + block))
                   : static_cast<loka::app::scene::NodeDefinitionBase *>(new (std::nothrow) TextDefinitionWithAttr(
                         Text(derivedStrings_[seat].state()) + style + block)))
            : 0;
    if (!out && JS_IsString(value))
    {
      loka::core::String text;
      treeString(ctx, tree, "text", text);
      out = new (std::nothrow) TextDefinitionWithAttr(Text(text) + style + block);
    }
    if (!out && !JS_IsString(value))
      fail("JavaScript Text requires a literal string or state seat.");
    JS_FreeValue(ctx, value);
    return out;
  }
  loka::app::scene::NodeDefinitionBase *JsCardNode::lowerEditText(JSContext *ctx, JSValueConst tree)
  {
    JSValue value = JS_GetPropertyStr(ctx, tree, "seat");
    int seat = -1;
    for (int i = 0; i < usedStates_; ++i)
      if (JS_IsStrictEqual(ctx, value, seats_[i]))
      {
        seat = i;
        break;
      }
    JS_FreeValue(ctx, value);
    if (seat < 0 || seatKinds_[seat] != 0)
    {
      fail("JavaScript EditText requires a String state seat.");
      return 0;
    }
    return new (std::nothrow) loka::app::EditText(strings_[seat]);
  }
  loka::app::scene::NodeDefinitionBase *JsCardNode::lowerButton(JSContext *ctx, JSValueConst tree)
  {
    loka::core::String label;
    JSValue handler = JS_GetPropertyStr(ctx, tree, "handler");
    int slot = JS_IsFunction(ctx, handler) ? handlerSlot(ctx, handler) : -1;
    JS_FreeValue(ctx, handler);
    if (!treeString(ctx, tree, "label", label) || slot < 0)
    {
      if (!failed_)
        fail("JavaScript Button requires a label and handler.");
      return 0;
    }
    loka::app::ButtonProps props;
    props.text(label);
    props.onClick(&emitters_[slot]);
    JSValue enabled = JS_GetPropertyStr(ctx, tree, "enabledSeat");
    if (!JS_IsUndefined(enabled))
    {
      int seat = -1;
      for (int i = 0; i < usedStates_; ++i)
        if (JS_IsStrictEqual(ctx, enabled, seats_[i]))
        {
          seat = i;
          break;
        }
      JS_FreeValue(ctx, enabled);
      if (seat < 0 || seatKinds_[seat] != 2)
      {
        fail("JavaScript Button enabled() requires a Bool state seat.");
        return 0;
      }
      props.enabled(bools_[seat].state());
    }
    else
      JS_FreeValue(ctx, enabled);
    return new (std::nothrow) loka::app::Button(props);
  }
  loka::app::scene::NodeDefinitionBase *JsCardNode::lowerChild(JSContext *ctx, JSValueConst tree, int depth)
  {
    return lower(ctx, tree, depth);
  }
  int JsCardNode::handlerSlot(JSContext *ctx, JSValueConst handler)
  {
    for (int i = 0; i < 8; ++i)
      if (JS_IsUndefined(handlers_[i]))
      {
        handlers_[i] = JS_DupValue(ctx, handler);
        return i;
      }
    fail("JavaScript card has more than 8 Button handlers.");
    return -1;
  }
  void JsCardNode::fire(int slot)
  {
    loka::core::String error;
    JSValue result = JS_UNDEFINED;
    props.runtime->setActive(this);
    if (!props.runtime->call(*this->engine_, handlers_[slot], instance_, 0, 0, result, error))
      error_.set(error);
    props.runtime->setActive(0);
    JS_FreeValue(this->engine_->context(), result);
  }
  void JsCardNode::fire0()
  {
    fire(0);
  }
  void JsCardNode::fire1()
  {
    fire(1);
  }
  void JsCardNode::fire2()
  {
    fire(2);
  }
  void JsCardNode::fire3()
  {
    fire(3);
  }
  void JsCardNode::fire4()
  {
    fire(4);
  }
  void JsCardNode::fire5()
  {
    fire(5);
  }
  void JsCardNode::fire6()
  {
    fire(6);
  }
  void JsCardNode::fire7()
  {
    fire(7);
  }
  CardScene *CreateCard(SmirkyCardId card, ScriptRuntime &runtime)
  {
    switch (card)
    {
    case SMIRKY_CARD_ERROR:
      return 0;
    case SMIRKY_CARD_FIRST:
    case SMIRKY_CARD_SECOND:
      break;
    }
    loka::core::OwnedDef<loka::app::scene::NodeDefinitionBase> root;
    root.reset(loka::app::scene::Boundary<JsCardNode>(JsCardProps(&runtime, card)).clone());
    if (!root.isSet())
      return 0;
    CardScene *scene = new (std::nothrow) CardScene(root.get());
    if (scene)
      root.take();
    return scene;
  }
} // namespace smirkycard
