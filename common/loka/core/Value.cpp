#include "loka/core/Value.hpp"

#include <cassert>
#include <new>

#include "loka/platform/StringUTF8.hpp"

namespace loka
{
  namespace core
  {
    Value::Value()
        : valueType(ValueTypeNull)
    {
    }

    Value::Value(bool b)
        : valueType(ValueTypeBool)
    {
      this->storage.boolValue = b;
    }

    Value::Value(long n)
        : valueType(ValueTypeInt)
    {
      this->storage.intValue = n;
    }

    Value::Value(double d)
        : valueType(ValueTypeDouble)
    {
      this->storage.doubleValue = d;
    }

    Value::Value(const String &s)
        : valueType(ValueTypeString)
    {
      new (this->storage.stringValue) String(s);
    }

    Value::Value(const Array &array)
        : valueType(ValueTypeArray)
    {
      new (this->storage.arrayValue) loka::core::Managed<ArrayStorage>(array.getHandle());
    }

    Value::Value(const Dictionary &dictionary)
        : valueType(ValueTypeDictionary)
    {
      new (this->storage.dictionaryValue) loka::core::Managed<DictionaryStorage>(dictionary.getHandle());
    }

    Value::Value(const Value &other)
        : valueType(other.valueType)
    {
      this->copyFrom(other);
    }

    Value &Value::operator=(const Value &other)
    {
      if (this != &other)
      {
        this->destroyActive();
        this->copyFrom(other);
      }
      return *this;
    }

    Value::~Value()
    {
      this->destroyActive();
    }

    const Value &Value::Null()
    {
      static const Value nullValue;
      return nullValue;
    }

    const Value &Value::True()
    {
      static const Value trueValue(true);
      return trueValue;
    }

    const Value &Value::False()
    {
      static const Value falseValue(false);
      return falseValue;
    }

    ValueType Value::type() const
    {
      return this->valueType;
    }

    bool Value::isNull() const
    {
      return this->valueType == ValueTypeNull;
    }

    bool Value::asBool(bool defaultValue) const
    {
      switch (this->valueType)
      {
      case ValueTypeBool:
        return this->storage.boolValue;
      case ValueTypeInt:
        return this->storage.intValue != 0;
      case ValueTypeDouble:
        return this->storage.doubleValue != 0.0;
      default:
        break;
      }
      return defaultValue;
    }

    long Value::asInt(long defaultValue) const
    {
      switch (this->valueType)
      {
      case ValueTypeInt:
        return this->storage.intValue;
      case ValueTypeBool:
        return this->storage.boolValue ? 1 : 0;
      case ValueTypeDouble:
        return static_cast<long>(this->storage.doubleValue);
      default:
        break;
      }
      return defaultValue;
    }

    double Value::asDouble(double defaultValue) const
    {
      switch (this->valueType)
      {
      case ValueTypeDouble:
        return this->storage.doubleValue;
      case ValueTypeInt:
        return static_cast<double>(this->storage.intValue);
      case ValueTypeBool:
        return this->storage.boolValue ? 1.0 : 0.0;
      default:
        break;
      }
      return defaultValue;
    }

    const String &Value::asString() const
    {
      assert(this->valueType == ValueTypeString && "Value::asString requires string type");
      return *this->stringSlot();
    }

    Array Value::asArray() const
    {
      if (this->valueType != ValueTypeArray)
        return Array();
      return Array(*this->arraySlot());
    }

    Dictionary Value::asDictionary() const
    {
      if (this->valueType != ValueTypeDictionary)
        return Dictionary();
      return Dictionary(*this->dictionarySlot());
    }

    void Value::copyFrom(const Value &other)
    {
      switch (other.valueType)
      {
      case ValueTypeNull:
        this->valueType = ValueTypeNull;
        break;
      case ValueTypeBool:
        this->storage.boolValue = other.storage.boolValue;
        this->valueType = ValueTypeBool;
        break;
      case ValueTypeInt:
        this->storage.intValue = other.storage.intValue;
        this->valueType = ValueTypeInt;
        break;
      case ValueTypeDouble:
        this->storage.doubleValue = other.storage.doubleValue;
        this->valueType = ValueTypeDouble;
        break;
      case ValueTypeString:
        new (this->storage.stringValue) String(*other.stringSlot());
        this->valueType = ValueTypeString;
        break;
      case ValueTypeArray:
        new (this->storage.arrayValue) loka::core::Managed<ArrayStorage>(*other.arraySlot());
        this->valueType = ValueTypeArray;
        break;
      case ValueTypeDictionary:
        new (this->storage.dictionaryValue) loka::core::Managed<DictionaryStorage>(*other.dictionarySlot());
        this->valueType = ValueTypeDictionary;
        break;
      }
    }

    void Value::destroyActive()
    {
      switch (this->valueType)
      {
      case ValueTypeString:
        this->stringSlot()->~String();
        break;
      case ValueTypeArray:
        this->arraySlot()->~Managed<ArrayStorage>();
        break;
      case ValueTypeDictionary:
        this->dictionarySlot()->~Managed<DictionaryStorage>();
        break;
      default:
        break;
      }
      this->valueType = ValueTypeNull;
    }

    String *Value::stringSlot()
    {
      return reinterpret_cast<String *>(this->storage.stringValue);
    }

    const String *Value::stringSlot() const
    {
      return reinterpret_cast<const String *>(this->storage.stringValue);
    }

    loka::core::Managed<ArrayStorage> *Value::arraySlot()
    {
      return reinterpret_cast<loka::core::Managed<ArrayStorage> *>(this->storage.arrayValue);
    }

    const loka::core::Managed<ArrayStorage> *Value::arraySlot() const
    {
      return reinterpret_cast<const loka::core::Managed<ArrayStorage> *>(this->storage.arrayValue);
    }

    loka::core::Managed<DictionaryStorage> *Value::dictionarySlot()
    {
      return reinterpret_cast<loka::core::Managed<DictionaryStorage> *>(this->storage.dictionaryValue);
    }

    const loka::core::Managed<DictionaryStorage> *Value::dictionarySlot() const
    {
      return reinterpret_cast<const loka::core::Managed<DictionaryStorage> *>(this->storage.dictionaryValue);
    }

    ArrayStorage::ArrayStorage()
        : values()
    {
    }

    Array::Array()
        : storage()
    {
    }

    Array::Array(const Array &other)
        : storage(other.storage)
    {
    }

    Array::Array(const loka::core::Managed<ArrayStorage> &handle)
        : storage(handle)
    {
    }

    Array &Array::operator=(const Array &other)
    {
      if (this != &other)
      {
        this->storage = other.storage;
      }
      return *this;
    }

    Array::~Array() {}

    void Array::ensureStorage()
    {
      if (this->storage.isValid())
        return;
      this->storage = loka::core::Managed<ArrayStorage>::Wrap(new ArrayStorage());
    }

    void Array::detach()
    {
      this->ensureStorage();
      if (this->storage.useCount() == 1)
        return;
      ArrayStorage *copy = new ArrayStorage();
      copy->values = this->storage->values;
      this->storage = loka::core::Managed<ArrayStorage>::Wrap(copy);
    }

    std::size_t Array::size() const
    {
      return this->storage.isValid() ? this->storage->values.size() : 0;
    }

    bool Array::empty() const
    {
      return this->size() == 0;
    }

    void Array::clear()
    {
      this->ensureStorage();
      this->detach();
      this->storage->values.clear();
    }

    void Array::pushBack(const Value &value)
    {
      this->ensureStorage();
      this->detach();
      this->storage->values.push_back(value);
    }

    const Value &Array::at(std::size_t index) const
    {
      assert(this->storage.isValid() && index < this->storage->values.size());
      return this->storage->values[index];
    }

    Value &Array::at(std::size_t index)
    {
      this->ensureStorage();
      assert(index < this->storage->values.size());
      this->detach();
      return this->storage->values[index];
    }

    Value &Array::operator[](std::size_t index)
    {
      return this->at(index);
    }

    const Value &Array::operator[](std::size_t index) const
    {
      return this->at(index);
    }

    loka::core::Managed<ArrayStorage> Array::getHandle() const
    {
      return this->storage;
    }

    DictionaryStorage::DictionaryStorage()
        : entries()
    {
    }

    Dictionary::Dictionary()
        : storage()
    {
    }

    Dictionary::Dictionary(const Dictionary &other)
        : storage(other.storage)
    {
    }

    Dictionary::Dictionary(const loka::core::Managed<DictionaryStorage> &handle)
        : storage(handle)
    {
    }

    Dictionary &Dictionary::operator=(const Dictionary &other)
    {
      if (this != &other)
      {
        this->storage = other.storage;
      }
      return *this;
    }

    Dictionary::~Dictionary() {}

    void Dictionary::ensureStorage()
    {
      if (this->storage.isValid())
        return;
      this->storage = loka::core::Managed<DictionaryStorage>::Wrap(new DictionaryStorage());
    }

    void Dictionary::detach()
    {
      this->ensureStorage();
      if (this->storage.useCount() == 1)
        return;
      DictionaryStorage *copy = new DictionaryStorage();
      copy->entries = this->storage->entries;
      this->storage = loka::core::Managed<DictionaryStorage>::Wrap(copy);
    }

    std::size_t Dictionary::size() const
    {
      return this->storage.isValid() ? this->storage->entries.size() : 0;
    }

    bool Dictionary::empty() const
    {
      return this->size() == 0;
    }

    void Dictionary::clear()
    {
      this->ensureStorage();
      this->detach();
      this->storage->entries.clear();
    }

    bool Dictionary::hasKey(const String &key) const
    {
      const std::string normalized = this->normalizeKey(key);
      return this->findEntry(normalized) != 0;
    }

    Value *Dictionary::find(const String &key)
    {
      if (!this->storage.isValid())
        return 0;
      this->detach();
      const std::string normalized = this->normalizeKey(key);
      DictionaryEntry *entry = this->findEntry(normalized);
      if (!entry)
        return 0;
      return &entry->value;
    }

    const Value *Dictionary::find(const String &key) const
    {
      const std::string normalized = this->normalizeKey(key);
      const DictionaryEntry *entry = this->findEntry(normalized);
      if (!entry)
        return 0;
      return &entry->value;
    }

    void Dictionary::set(const String &key, const Value &value)
    {
      this->ensureStorage();
      this->detach();
      const std::string normalized = this->normalizeKey(key);
      DictionaryEntry *entry = this->findEntry(normalized);
      if (entry)
      {
        entry->logicalKey = key;
        entry->utf8Key = normalized;
        entry->value = value;
      }
      else
      {
        DictionaryEntry newEntry;
        newEntry.logicalKey = key;
        newEntry.utf8Key = normalized;
        newEntry.value = value;
        this->storage->entries.push_back(newEntry);
      }
    }

    bool Dictionary::remove(const String &key)
    {
      if (!this->storage.isValid())
        return false;
      this->detach();
      const std::string normalized = this->normalizeKey(key);
      std::vector<DictionaryEntry> &entries = this->storage->entries;
      for (std::vector<DictionaryEntry>::iterator it = entries.begin(); it != entries.end(); ++it)
      {
        if (it->utf8Key == normalized)
        {
          entries.erase(it);
          return true;
        }
      }
      return false;
    }

    std::string Dictionary::normalizeKey(const String &key) const
    {
      std::string normalized;
      if (!platform::CollectUtf8(key, normalized))
        normalized.clear();
      return normalized;
    }

    DictionaryEntry *Dictionary::findEntry(const std::string &utf8Key)
    {
      if (!this->storage.isValid())
        return 0;
      std::vector<DictionaryEntry> &entries = this->storage->entries;
      for (std::size_t i = 0; i < entries.size(); ++i)
      {
        if (entries[i].utf8Key == utf8Key)
          return &entries[i];
      }
      return 0;
    }

    const DictionaryEntry *Dictionary::findEntry(const std::string &utf8Key) const
    {
      if (!this->storage.isValid())
        return 0;
      const std::vector<DictionaryEntry> &entries = this->storage->entries;
      for (std::size_t i = 0; i < entries.size(); ++i)
      {
        if (entries[i].utf8Key == utf8Key)
          return &entries[i];
      }
      return 0;
    }

    loka::core::Managed<DictionaryStorage> Dictionary::getHandle() const
    {
      return this->storage;
    }

  } // namespace core
} // namespace loka
