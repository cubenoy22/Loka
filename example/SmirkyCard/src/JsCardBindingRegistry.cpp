#include "JsCardBindingRegistry.hpp"
#include "CardNodes.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include <new>

namespace smirkycard
{
  struct JsCardBindingRegistry::LoweringEntry
  {
    LoweringEntry(IJsNodeLowering *value)
        : value_(value),
          next_(0)
    {
    }
    IJsNodeLowering *value_;
    LoweringEntry *next_;
  };
  struct JsCardBindingRegistry::GlobalEntry
  {
    GlobalEntry(const IJsGlobal &value)
        : value_(value),
          next_(0)
    {
    }
    IJsGlobal value_;
    GlobalEntry *next_;
  };
  JsCardBindingRegistry::~JsCardBindingRegistry()
  {
    while (lowerings_)
    {
      LoweringEntry *next = lowerings_->next_;
      delete lowerings_->value_;
      delete lowerings_;
      lowerings_ = next;
    }
    while (globals_)
    {
      GlobalEntry *next = globals_->next_;
      delete globals_;
      globals_ = next;
    }
  }
  bool JsCardBindingRegistry::registerLowering(IJsNodeLowering *lowering)
  {
    if (!lowering || !lowering->name || lowering->kind)
      return false;
    for (LoweringEntry *entry = lowerings_; entry; entry = entry->next_)
      if (entry->value_->kind == nextKind_ || !strcmp(entry->value_->name, lowering->name))
        return false;
    LoweringEntry *entry = new (std::nothrow) LoweringEntry(lowering);
    if (!entry)
      return false;
    lowering->kind = nextKind_++;
    entry->next_ = lowerings_;
    lowerings_ = entry;
    return true;
  }
  bool JsCardBindingRegistry::registerGlobal(const IJsGlobal &global)
  {
    if (!global.name || !global.fn || global.length < 0)
      return false;
    for (GlobalEntry *entry = globals_; entry; entry = entry->next_)
      if (!strcmp(entry->value_.name, global.name))
        return false;
    for (LoweringEntry *entry = lowerings_; entry; entry = entry->next_)
      if (!strcmp(entry->value_->name, global.name))
        return false;
    GlobalEntry *entry = new (std::nothrow) GlobalEntry(global);
    if (!entry)
      return false;
    entry->next_ = globals_;
    globals_ = entry;
    return true;
  }
  IJsNodeLowering *JsCardBindingRegistry::lookup(int kind) const
  {
    for (LoweringEntry *entry = lowerings_; entry; entry = entry->next_)
      if (entry->value_->kind == kind)
        return entry->value_;
    return 0;
  }
  bool JsCardBindingRegistry::install(JSContext *context) const
  {
    JSValue global = JS_GetGlobalObject(context);
    for (LoweringEntry *entry = lowerings_; entry; entry = entry->next_)
      JS_SetPropertyStr(context,
                        global,
                        entry->value_->name,
                        JS_NewCFunctionMagic(context,
                                             &JsCardBindingRegistry::build,
                                             entry->value_->name,
                                             1,
                                             JS_CFUNC_generic_magic,
                                             entry->value_->kind));
    for (GlobalEntry *entry = globals_; entry; entry = entry->next_)
      JS_SetPropertyStr(context,
                        global,
                        entry->value_.name,
                        JS_NewCFunction(context, entry->value_.fn, entry->value_.name, entry->value_.length));
    JS_FreeValue(context, global);
    return true;
  }
  JSValue JsCardBindingRegistry::build(JSContext *context, JSValueConst, int argc, JSValueConst *argv, int magic)
  {
    ScriptRuntime *runtime = static_cast<ScriptRuntime *>(JS_GetContextOpaque(context));
    IJsNodeLowering *lowering = runtime ? runtime->registry_.lookup(magic) : 0;
    return lowering ? lowering->build(context, argc, argv) : JS_ThrowTypeError(context, "unknown tree helper");
  }
  namespace
  {
    JSValue newTree(JSContext *ctx, int kind)
    {
      JSValue node = JS_NewObject(ctx);
      JS_DefinePropertyValueStr(ctx, node, "kind", JS_NewInt32(ctx, kind), JS_PROP_ENUMERABLE);
      JS_SetPropertyStr(ctx, node, "TEST_ID", JS_NewCFunction(ctx, &ScriptRuntime::testId, "TEST_ID", 1));
      return node;
    }
    JSValue buildStack(JSContext *ctx, int kind, const char *name, int argc, JSValueConst *argv)
    {
      JSValue node = newTree(ctx, kind);
      JSValue children = JS_NewArray(ctx);
      uint32_t count = 0;
      for (int i = 0; i < argc; ++i)
      {
        int64_t length = 0;
        const bool array = JS_IsArray(argv[i]);
        if (array && JS_GetLength(ctx, argv[i], &length))
        {
          JS_FreeValue(ctx, children);
          JS_FreeValue(ctx, node);
          return JS_ThrowTypeError(ctx, "%s children must be tree nodes", name);
        }
        const uint32_t entries = array ? static_cast<uint32_t>(length) : 1;
        for (uint32_t j = 0; j < entries; ++j)
        {
          JSValue child = array ? JS_GetPropertyUint32(ctx, argv[i], j) : JS_DupValue(ctx, argv[i]);
          JSValue childKind = JS_GetPropertyStr(ctx, child, "kind");
          if (!JS_IsObject(child) || JS_IsArray(child) || !JS_IsNumber(childKind))
          {
            JS_FreeValue(ctx, childKind);
            JS_FreeValue(ctx, child);
            JS_FreeValue(ctx, children);
            JS_FreeValue(ctx, node);
            return JS_ThrowTypeError(ctx, "%s children must be tree nodes (nested arrays are not allowed)", name);
          }
          JS_FreeValue(ctx, childKind);
          if (count == 16)
          {
            JS_FreeValue(ctx, child);
            JS_FreeValue(ctx, children);
            JS_FreeValue(ctx, node);
            return JS_ThrowRangeError(ctx, "%s accepts at most 16 children", name);
          }
          JS_SetPropertyUint32(ctx, children, count++, child);
        }
      }
      JS_SetPropertyStr(ctx, node, "children", children);
      JS_FreezeObject(ctx, node);
      return node;
    }
    class StackLowering : public IJsNodeLowering
    {
    public:
      StackLowering(const char *n, bool row)
          : row_(row)
      {
        name = n;
        kind = 0;
      }
      virtual JSValue build(JSContext *ctx, int argc, JSValueConst *argv)
      {
        return buildStack(ctx, kind, name, argc, argv);
      }
      virtual loka::app::scene::NodeDefinitionBase *
      lower(JsCardNode &node, JSContext *ctx, JSValueConst tree, int depth)
      {
        loka::app::scene::NodeDefinitionBase *stack = 0;
        loka::app::scene::INestableDefinition *nest = 0;
        if (row_)
        {
          loka::app::Row *row = new (std::nothrow) loka::app::Row();
          stack = row;
          nest = row;
        }
        else
        {
          loka::app::VStack *column = new (std::nothrow) loka::app::VStack();
          stack = column;
          nest = column;
        }
        JSValue children = JS_GetPropertyStr(ctx, tree, "children");
        int64_t length = 0;
        if (!stack || !JS_IsArray(children) || JS_GetLength(ctx, children, &length) || length > 16)
        {
          JS_FreeValue(ctx, children);
          delete stack;
          node.fail("JavaScript stack has invalid children.");
          return 0;
        }
        for (uint32_t i = 0; i < static_cast<uint32_t>(length); ++i)
        {
          JSValue child = JS_GetPropertyUint32(ctx, children, i);
          loka::app::scene::NodeDefinitionBase *definition = node.lowerChild(ctx, child, depth + 1);
          JS_FreeValue(ctx, child);
          if (!definition)
          {
            JS_FreeValue(ctx, children);
            delete stack;
            return 0;
          }
          nest->addOwnedChild(definition);
        }
        JS_FreeValue(ctx, children);
        return stack;
      }

    private:
      bool row_;
    };
    class TextLowering : public IJsNodeLowering
    {
    public:
      TextLowering()
      {
        name = "Text";
        kind = 0;
      }
      virtual JSValue build(JSContext *ctx, int argc, JSValueConst *argv)
      {
        if (argc != 1)
          return JS_ThrowTypeError(ctx, "Text(value) requires one value");
        JSValue n = newTree(ctx, kind);
        JS_SetPropertyStr(ctx, n, "text", JS_DupValue(ctx, argv[0]));
        JS_FreezeObject(ctx, n);
        return n;
      }
      virtual loka::app::scene::NodeDefinitionBase *lower(JsCardNode &node, JSContext *ctx, JSValueConst tree, int)
      {
        return node.lowerText(ctx, tree);
      }
    };
    class EditTextLowering : public IJsNodeLowering
    {
    public:
      EditTextLowering()
      {
        name = "EditText";
        kind = 0;
      }
      virtual JSValue build(JSContext *ctx, int argc, JSValueConst *argv)
      {
        if (argc != 1)
          return JS_ThrowTypeError(ctx, "EditText(seat) requires one seat");
        JSValue n = newTree(ctx, kind);
        JS_SetPropertyStr(ctx, n, "seat", JS_DupValue(ctx, argv[0]));
        JS_FreezeObject(ctx, n);
        return n;
      }
      virtual loka::app::scene::NodeDefinitionBase *lower(JsCardNode &node, JSContext *ctx, JSValueConst tree, int)
      {
        return node.lowerEditText(ctx, tree);
      }
    };
    class ButtonLowering : public IJsNodeLowering
    {
    public:
      ButtonLowering()
      {
        name = "Button";
        kind = 0;
      }
      virtual JSValue build(JSContext *ctx, int argc, JSValueConst *argv)
      {
        if (argc != 2 || !JS_IsString(argv[0]) || !JS_IsFunction(ctx, argv[1]))
          return JS_ThrowTypeError(ctx, "Button(label, handler) requires a string and function");
        JSValue n = newTree(ctx, kind);
        JS_SetPropertyStr(ctx, n, "label", JS_DupValue(ctx, argv[0]));
        JS_SetPropertyStr(ctx, n, "handler", JS_DupValue(ctx, argv[1]));
        JS_SetPropertyStr(ctx, n, "enabled", JS_NewCFunction(ctx, &ScriptRuntime::enabled, "enabled", 1));
        JS_FreezeObject(ctx, n);
        return n;
      }
      virtual loka::app::scene::NodeDefinitionBase *lower(JsCardNode &node, JSContext *ctx, JSValueConst tree, int)
      {
        return node.lowerButton(ctx, tree);
      }
    };
  } // namespace
  bool RegisterSmirkyCardBindings(JsCardBindingRegistry &registry)
  {
    IJsGlobal card = {"card", &ScriptRuntime::card, 2};
    IJsGlobal state = {"state", &ScriptRuntime::state, 1};
    IJsGlobal go = {"go", &ScriptRuntime::go, 1};
    IJsGlobal reload = {"reload", &ScriptRuntime::reload, 0};
    return registry.registerLowering(new (std::nothrow) StackLowering("VStack", false))
           && registry.registerLowering(new (std::nothrow) TextLowering())
           && registry.registerLowering(new (std::nothrow) EditTextLowering())
           && registry.registerLowering(new (std::nothrow) ButtonLowering())
           && registry.registerLowering(new (std::nothrow) StackLowering("Row", true)) && registry.registerGlobal(card)
           && registry.registerGlobal(state) && registry.registerGlobal(go) && registry.registerGlobal(reload);
  }
} // namespace smirkycard
