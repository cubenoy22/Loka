#include "JsCardBindingRegistry.hpp"
#include "CardNodes.hpp"
#include "SmirkyMarkup.hpp"
#include "app/nodes/AttributedText.hpp"
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
        if (argc < 1 || argc > 3)
          return JS_ThrowTypeError(ctx, "Text(value, style?, block?) requires a value and optional style and block");
        loka::app::TextStyle style;
        if (argc >= 2 && !JS_IsUndefined(argv[1]) && !readTextStyle(ctx, argv[1], style))
          return JS_EXCEPTION;
        loka::app::BlockStyle block;
        if (argc == 3 && !JS_IsUndefined(argv[2]) && !readBlockStyle(ctx, argv[2], block))
          return JS_EXCEPTION;
        JSValue n = newTree(ctx, kind);
        if (JS_IsException(n))
          return n;
        if (JS_SetPropertyStr(ctx, n, "text", JS_DupValue(ctx, argv[0])) < 0
            || (argc >= 2 && JS_SetPropertyStr(ctx, n, "style", JS_DupValue(ctx, argv[1])) < 0)
            || (argc == 3 && JS_SetPropertyStr(ctx, n, "block", JS_DupValue(ctx, argv[2])) < 0)
            || JS_FreezeObject(ctx, n) < 0)
        {
          JS_FreeValue(ctx, n);
          return JS_EXCEPTION;
        }
        return n;
      }
      virtual loka::app::scene::NodeDefinitionBase *lower(JsCardNode &node, JSContext *ctx, JSValueConst tree, int)
      {
        return node.lowerText(ctx, tree);
      }
    };
    bool readMarkup(JSContext *ctx, JSValueConst value, JSValueConst dict, loka::app::AttributedString &out)
    {
      if (!JS_IsString(value))
      {
        JS_ThrowTypeError(ctx, "Markup requires a literal string; state seats are not supported");
        return false;
      }
      loka::app::TextStyle style;
      if (JS_IsException(dict) || (!JS_IsUndefined(dict) && !readTextStyle(ctx, dict, style)))
        return false;
      size_t length = 0;
      const char *bytes = JS_ToCStringLen(ctx, &length, value);
      if (!bytes)
        return false;
      const bool valid = ParseSmirkyMarkup(bytes, length, style, out);
      JS_FreeCString(ctx, bytes);
      if (!valid)
        JS_ThrowTypeError(ctx, "Markup parse or allocation refused");
      return valid;
    }

    class MarkupLowering : public IJsNodeLowering
    {
    public:
      MarkupLowering()
      {
        name = "Markup";
        kind = 0;
      }
      virtual JSValue build(JSContext *ctx, int argc, JSValueConst *argv)
      {
        if (argc < 1 || argc > 3)
          return JS_ThrowTypeError(ctx,
                                   "Markup(markup, style?, block?) requires a string and optional style and block");
        loka::app::AttributedString parsed;
        if (!readMarkup(ctx, argv[0], argc >= 2 ? argv[1] : JS_UNDEFINED, parsed))
          return JS_EXCEPTION;
        loka::app::BlockStyle block;
        if (argc == 3 && !JS_IsUndefined(argv[2]) && !readBlockStyle(ctx, argv[2], block))
          return JS_EXCEPTION;
        JSValue n = newTree(ctx, kind);
        if (JS_IsException(n))
          return n;
        if (JS_SetPropertyStr(ctx, n, "markup", JS_DupValue(ctx, argv[0])) < 0
            || (argc >= 2 && JS_SetPropertyStr(ctx, n, "style", JS_DupValue(ctx, argv[1])) < 0)
            || (argc == 3 && JS_SetPropertyStr(ctx, n, "block", JS_DupValue(ctx, argv[2])) < 0)
            || JS_FreezeObject(ctx, n) < 0)
        {
          JS_FreeValue(ctx, n);
          return JS_EXCEPTION;
        }
        return n;
      }
      virtual loka::app::scene::NodeDefinitionBase *lower(JsCardNode &node, JSContext *ctx, JSValueConst tree, int)
      {
        JSValue value = JS_GetPropertyStr(ctx, tree, "markup");
        JSValue dict = JS_GetPropertyStr(ctx, tree, "style");
        loka::app::AttributedString parsed;
        bool valid = readMarkup(ctx, value, dict, parsed);
        JS_FreeValue(ctx, dict);
        JS_FreeValue(ctx, value);
        loka::app::BlockStyle block;
        if (valid)
        {
          dict = JS_GetPropertyStr(ctx, tree, "block");
          valid = !JS_IsException(dict) && (JS_IsUndefined(dict) || readBlockStyle(ctx, dict, block));
          JS_FreeValue(ctx, dict);
        }
        if (!valid)
        {
          JS_FreeValue(ctx, JS_GetException(ctx));
          node.fail("JavaScript Markup parse, style, block or allocation refused.");
          return 0;
        }
        return new (std::nothrow)
            loka::app::AttributedTextDefinitionWithAttr(loka::app::AttributedText(parsed) + block);
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
           && registry.registerLowering(new (std::nothrow) StackLowering("Row", true))
           && registry.registerLowering(new (std::nothrow) MarkupLowering()) && registry.registerGlobal(card)
           && registry.registerGlobal(state) && registry.registerGlobal(go) && registry.registerGlobal(reload);
  }
} // namespace smirkycard
