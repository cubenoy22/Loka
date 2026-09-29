#include "CardCarry.hpp"
#include "JsOwnProperties.hpp"
#include <cfloat>
#include <cstdlib>
#include <cstring>
#include <math.h>

namespace smirkycard
{
  namespace
  {
    /** Owns descriptor values even when validation or recursive encoding refuses. */
    class DataProperty
    {
    public:
      explicit DataProperty(JSContext *ctx)
          : ctx_(ctx)
      {
        this->desc_.flags = 0;
        this->desc_.value = this->desc_.getter = this->desc_.setter = JS_UNDEFINED;
      }
      ~DataProperty()
      {
        JS_FreeValue(this->ctx_, this->desc_.value);
        JS_FreeValue(this->ctx_, this->desc_.getter);
        JS_FreeValue(this->ctx_, this->desc_.setter);
      }
      bool read(JSValueConst object, JSAtom atom)
      {
        return JS_GetOwnProperty(this->ctx_, &this->desc_, object, atom) == 1;
      }
      bool enumerableData() const
      {
        return (this->desc_.flags & JS_PROP_ENUMERABLE) && !(this->desc_.flags & JS_PROP_GETSET);
      }
      JSValue value() const
      {
        return this->desc_.value;
      }

    private:
      JSContext *ctx_;
      JSPropertyDescriptor desc_;
      DataProperty(const DataProperty &);
      DataProperty &operator=(const DataProperty &);
    };

    /** Stack-local bounded construction; no JS reference survives encode(). */
    class CarryEncoder
    {
      struct Ancestor
      {
        JSValueConst value;
        const Ancestor *parent;
      };

    public:
      explicit CarryEncoder(JSContext *ctx)
          : ctx_(ctx),
            bytes_(static_cast<char *>(std::malloc(kCarryByteBudget + 1))),
            used_(0),
            entries_(0)
      {
      }
      ~CarryEncoder()
      {
        std::free(this->bytes_);
      }
      char *encode(JSValueConst value)
      {
        if (!this->bytes_)
        {
          JS_ThrowOutOfMemory(this->ctx_);
          return 0;
        }
        if (!this->write(value, 0, 0))
          return 0;
        this->bytes_[this->used_] = 0;
        // Shrinking is optional; a refused shrink leaves the bounded allocation valid.
        char *compact = static_cast<char *>(std::realloc(this->bytes_, this->used_ + 1));
        char *result = compact ? compact : this->bytes_;
        this->bytes_ = 0;
        return result;
      }

    private:
      bool typeError()
      {
        JS_ThrowTypeError(this->ctx_, "carry requires JSON-serializable data without cycles");
        return false;
      }
      bool rangeError()
      {
        JS_ThrowRangeError(this->ctx_, "carry exceeds #1032 byte, depth or entry budget");
        return false;
      }
      bool append(const char *bytes, size_t size)
      {
        if (size > kCarryByteBudget - this->used_)
          return this->rangeError();
        std::memcpy(this->bytes_ + this->used_, bytes, size);
        this->used_ += size;
        return true;
      }
      bool token(const char *s)
      {
        return this->append(s, std::strlen(s));
      }
      bool string(JSValueConst value)
      {
        int64_t length = 0;
        if (JS_GetLength(this->ctx_, value, &length) < 0)
          return false;
        if (length < 0 || static_cast<uint64_t>(length) > kCarryByteBudget - this->used_)
          return this->rangeError();
        size_t count = 0;
        const uint16_t *units = JS_ToCStringLenUTF16(this->ctx_, &count, value);
        if (!units)
          return false;
        bool ok = this->token("\"");
        for (size_t i = 0; ok && i < count; ++i)
        {
          unsigned c = units[i];
          char bytes[6];
          size_t n = 0;
          if (c == '"' || c == '\\')
          {
            bytes[n++] = '\\';
            bytes[n++] = static_cast<char>(c);
          }
          else if (c < 32 || (c >= 0xd800 && c <= 0xdfff))
          {
            // Escaping UTF-16 surrogates preserves pairs and lone surrogates alike.
            const char *hex = "0123456789abcdef";
            bytes[n++] = '\\';
            bytes[n++] = 'u';
            for (int shift = 12; shift >= 0; shift -= 4)
              bytes[n++] = hex[(c >> shift) & 15];
          }
          else if (c < 0x80)
            bytes[n++] = static_cast<char>(c);
          else if (c < 0x800)
          {
            bytes[n++] = static_cast<char>(0xc0 | (c >> 6));
            bytes[n++] = static_cast<char>(0x80 | (c & 63));
          }
          else
          {
            bytes[n++] = static_cast<char>(0xe0 | (c >> 12));
            bytes[n++] = static_cast<char>(0x80 | ((c >> 6) & 63));
            bytes[n++] = static_cast<char>(0x80 | (c & 63));
          }
          ok = this->append(bytes, n);
        }
        JS_FreeCStringUTF16(this->ctx_, units);
        return ok && this->token("\"");
      }
      bool write(JSValueConst value, unsigned depth, const Ancestor *parent)
      {
        if (JS_IsNull(value))
          return this->token("null");
        if (JS_IsBool(value))
          return this->token(JS_ToBool(this->ctx_, value) ? "true" : "false");
        if (JS_IsString(value))
          return this->string(value);
        if (JS_IsNumber(value))
        {
          double number = 0;
          if (JS_ToFloat64(this->ctx_, &number, value) < 0)
            return false;
          if (!(number >= -DBL_MAX && number <= DBL_MAX))
            return this->typeError();
          if (number == 0 && signbit(number))
            return this->token("-0");
          const char *text = JS_ToCString(this->ctx_, value);
          if (!text)
            return false;
          bool ok = this->token(text);
          JS_FreeCString(this->ctx_, text);
          return ok;
        }
        if (!JS_IsObject(value) || JS_IsProxy(value))
          return this->typeError();
        for (const Ancestor *p = parent; p; p = p->parent)
          if (JS_VALUE_GET_PTR(value) == JS_VALUE_GET_PTR(p->value))
            return this->typeError();
        if (depth == kCarryDepthBudget)
          return this->rangeError();
        const bool array = JS_IsArray(value);
        if (!array)
        {
          JSValue plain = JS_NewObject(this->ctx_);
          if (JS_IsException(plain))
            return false;
          JSClassID plainClass = JS_GetClassID(plain);
          JS_FreeValue(this->ctx_, plain);
          if (JS_GetClassID(value) != plainClass)
            return this->typeError();
        }
        JSValue proto = JS_GetPrototype(this->ctx_, value);
        JSValue intrinsic = JS_GetClassProto(this->ctx_, JS_GetClassID(value));
        bool plainPrototype = (!array && JS_IsNull(proto))
                              || (JS_IsObject(proto) && JS_VALUE_GET_PTR(proto) == JS_VALUE_GET_PTR(intrinsic));
        JS_FreeValue(this->ctx_, proto);
        JS_FreeValue(this->ctx_, intrinsic);
        if (!plainPrototype)
          return this->typeError();
        JsOwnProperties keys(this->ctx_);
        if (!keys.read(value, JS_GPN_STRING_MASK | JS_GPN_SYMBOL_MASK))
          return false;
        int64_t length = 0;
        if (array && JS_GetLength(this->ctx_, value, &length) < 0)
          return false;
        if (array && (length < 0 || static_cast<uint64_t>(length) + 1 != keys.count()))
          return this->typeError();
        const uint32_t count = keys.count() - (array ? 1 : 0);
        if (count > kCarryEntryBudget - this->entries_)
          return this->rangeError();
        const Ancestor ancestor = {value, parent};
        if (!this->token(array ? "[" : "{"))
          return false;
        for (uint32_t i = 0; i < keys.count(); ++i)
        {
          JSValue key = JS_AtomToValue(this->ctx_, keys.atom(i));
          if (JS_IsException(key))
            return false;
          if (JS_IsSymbol(key))
          {
            JS_FreeValue(this->ctx_, key);
            return this->typeError();
          }
          if (array)
          {
            JSAtom expected = i == count ? JS_NewAtom(this->ctx_, "length") : JS_NewAtomUInt32(this->ctx_, i);
            bool match = expected == keys.atom(i);
            JS_FreeAtom(this->ctx_, expected);
            JS_FreeValue(this->ctx_, key);
            if (!match)
              return this->typeError();
            if (i == count)
              continue;
          }
          DataProperty property(this->ctx_);
          bool ok = property.read(value, keys.atom(i));
          if (ok && !property.enumerableData())
            ok = this->typeError();
          if (ok && this->entries_ == kCarryEntryBudget)
            ok = this->rangeError();
          if (ok)
          {
            ++this->entries_;
            ok = (i == 0 || this->token(","));
            if (!array)
              ok = ok && this->string(key) && this->token(":");
            ok = ok && this->write(property.value(), depth + 1, &ancestor);
          }
          if (!array)
            JS_FreeValue(this->ctx_, key);
          if (!ok)
            return false;
        }
        return this->token(array ? "]" : "}");
      }
      JSContext *ctx_;
      char *bytes_;
      size_t used_;
      unsigned entries_;
      CarryEncoder(const CarryEncoder &);
      CarryEncoder &operator=(const CarryEncoder &);
    };
  } // namespace
  void CardCarry::release(char *bytes, void *)
  {
    std::free(bytes);
  }
  bool CardCarry::encode(JSContext *ctx, JSValueConst value, CardCarry &result)
  {
    if (JS_IsUndefined(value))
    {
      result = CardCarry();
      return true;
    }
    CarryEncoder encoder(ctx);
    char *bytes = encoder.encode(value);
    if (!bytes)
      return false;
    loka::core::Managed<char> owned = loka::core::Managed<char>::TryWrap(bytes, release, 0);
    if (!owned.isValid())
    {
      std::free(bytes);
      JS_ThrowOutOfMemory(ctx);
      return false;
    }
    result.bytes_ = owned;
    return true;
  }
  JSValue CardCarry::decode(JSContext *ctx) const
  {
    const char *bytes = this->bytes_.get();
    return bytes ? JS_ParseJSON(ctx, bytes, std::strlen(bytes), "<card carry>") : JS_UNDEFINED;
  }
  bool CardCarry::operator<(const CardCarry &other) const
  {
    return std::strcmp(this->bytes_.isValid() ? this->bytes_.get() : "",
                       other.bytes_.isValid() ? other.bytes_.get() : "")
           < 0;
  }
} // namespace smirkycard
