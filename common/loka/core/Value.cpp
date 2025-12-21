#include "loka/core/Value.hpp"

#include <cassert>

#include "loka/platform/StringUTF8.hpp"

namespace loka
{
  namespace core
  {
    Value::Value() : valueType(ValueTypeNull), boolValue(false), intValue(0), doubleValue(0.0), stringValue(), arrayStorage(), dictionaryStorage()
    {
    }

    Value::Value(bool b) : valueType(ValueTypeBool), boolValue(b), intValue(b ? 1 : 0), doubleValue(b ? 1.0 : 0.0), stringValue(), arrayStorage(), dictionaryStorage()
    {
    }

    Value::Value(long n) : valueType(ValueTypeInt), boolValue(n != 0), intValue(n), doubleValue(static_cast<double>(n)), stringValue(), arrayStorage(), dictionaryStorage()
    {
    }

    Value::Value(double d) : valueType(ValueTypeDouble), boolValue(d != 0.0), intValue(static_cast<long>(d)), doubleValue(d), stringValue(), arrayStorage(), dictionaryStorage()
    {
    }

    Value::Value(const String &s) : valueType(ValueTypeString), boolValue(false), intValue(0), doubleValue(0.0), stringValue(s), arrayStorage(), dictionaryStorage()
    {
    }

    Value::Value(const Array &array) : valueType(ValueTypeNull), boolValue(false), intValue(0), doubleValue(0.0), stringValue(), arrayStorage(), dictionaryStorage()
    {
      this->setArray(array);
    }

    Value::Value(const Dictionary &dictionary) : valueType(ValueTypeNull), boolValue(false), intValue(0), doubleValue(0.0), stringValue(), arrayStorage(), dictionaryStorage()
    {
      this->setDictionary(dictionary);
    }

    Value::Value(const Value &other) : valueType(ValueTypeNull), boolValue(false), intValue(0), doubleValue(0.0), stringValue(), arrayStorage(), dictionaryStorage()
    {
      this->copyFrom(other);
    }

    Value &Value::operator=(const Value &other)
    {
      if (this != &other)
      {
        this->releaseRefs();
        this->copyFrom(other);
      }
      return *this;
    }

    Value::~Value()
    {
      this->releaseRefs();
    }

    Value Value::Null()
    {
      return Value();
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
        return this->boolValue;
      case ValueTypeInt:
        return this->intValue != 0;
      case ValueTypeDouble:
        return this->doubleValue != 0.0;
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
        return this->intValue;
      case ValueTypeBool:
        return this->boolValue ? 1 : 0;
      case ValueTypeDouble:
        return static_cast<long>(this->doubleValue);
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
        return this->doubleValue;
      case ValueTypeInt:
        return static_cast<double>(this->intValue);
      case ValueTypeBool:
        return this->boolValue ? 1.0 : 0.0;
      default:
        break;
      }
      return defaultValue;
    }

    const String &Value::asString() const
    {
      assert(this->valueType == ValueTypeString && "Value::asString requires string type");
      return this->stringValue;
    }

    Array Value::asArray() const
    {
      if (this->valueType != ValueTypeArray || !this->arrayStorage.isValid())
        return Array();
      return Array(this->arrayStorage);
    }

    Dictionary Value::asDictionary() const
    {
      if (this->valueType != ValueTypeDictionary || !this->dictionaryStorage.isValid())
        return Dictionary();
      return Dictionary(this->dictionaryStorage);
    }

    void Value::setBool(bool b)
    {
      this->releaseRefs();
      this->valueType = ValueTypeBool;
      this->boolValue = b;
      this->intValue = b ? 1 : 0;
      this->doubleValue = b ? 1.0 : 0.0;
      this->stringValue = String();
    }

    void Value::setInt(long n)
    {
      this->releaseRefs();
      this->valueType = ValueTypeInt;
      this->intValue = n;
      this->boolValue = n != 0;
      this->doubleValue = static_cast<double>(n);
      this->stringValue = String();
    }

    void Value::setDouble(double d)
    {
      this->releaseRefs();
      this->valueType = ValueTypeDouble;
      this->doubleValue = d;
      this->boolValue = d != 0.0;
      this->intValue = static_cast<long>(d);
      this->stringValue = String();
    }

    void Value::setString(const String &s)
    {
      this->releaseRefs();
      this->valueType = ValueTypeString;
      this->stringValue = s;
      this->boolValue = false;
      this->intValue = 0;
      this->doubleValue = 0.0;
    }

    void Value::setArray(const Array &array)
    {
      this->releaseRefs();
      this->valueType = ValueTypeArray;
      this->arrayStorage = array.getHandle();
      this->dictionaryStorage.reset();
    }

    void Value::setDictionary(const Dictionary &dictionary)
    {
      this->releaseRefs();
      this->valueType = ValueTypeDictionary;
      this->dictionaryStorage = dictionary.getHandle();
      this->arrayStorage.reset();
    }

    void Value::reset()
    {
      this->releaseRefs();
      this->valueType = ValueTypeNull;
      this->boolValue = false;
      this->intValue = 0;
      this->doubleValue = 0.0;
      this->stringValue = String();
    }

    void Value::releaseRefs()
    {
      if (this->valueType == ValueTypeArray)
        this->arrayStorage.reset();
      else if (this->valueType == ValueTypeDictionary)
        this->dictionaryStorage.reset();
    }

    void Value::copyFrom(const Value &other)
    {
      this->valueType = other.valueType;
      this->boolValue = other.boolValue;
      this->intValue = other.intValue;
      this->doubleValue = other.doubleValue;
      this->stringValue = other.stringValue;
      this->arrayStorage = Managed<ArrayStorage>();
      this->dictionaryStorage = Managed<DictionaryStorage>();
      if (other.valueType == ValueTypeArray)
      {
        this->arrayStorage = other.arrayStorage;
      }
      else if (other.valueType == ValueTypeDictionary)
      {
        this->dictionaryStorage = other.dictionaryStorage;
      }
    }

    ArrayStorage::ArrayStorage() : values()
    {
    }

    Array::Array() : storage()
    {
    }

    Array::Array(const Array &other) : storage(other.storage)
    {
    }

    Array::Array(const Managed<ArrayStorage> &handle) : storage(handle)
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

    Array::~Array()
    {
    }

    void Array::ensureStorage()
    {
      if (this->storage.isValid())
        return;
      this->storage = Managed<ArrayStorage>::Wrap(new ArrayStorage());
    }

    void Array::detach()
    {
      this->ensureStorage();
      if (this->storage.useCount() == 1)
        return;
      ArrayStorage *copy = new ArrayStorage();
      copy->values = this->storage->values;
      this->storage = Managed<ArrayStorage>::Wrap(copy);
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

    Managed<ArrayStorage> Array::getHandle() const
    {
      return this->storage;
    }

    DictionaryStorage::DictionaryStorage() : entries()
    {
    }

    Dictionary::Dictionary() : storage()
    {
    }

    Dictionary::Dictionary(const Dictionary &other) : storage(other.storage)
    {
    }

    Dictionary::Dictionary(const Managed<DictionaryStorage> &handle) : storage(handle)
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

    Dictionary::~Dictionary()
    {
    }

    void Dictionary::ensureStorage()
    {
      if (this->storage.isValid())
        return;
      this->storage = Managed<DictionaryStorage>::Wrap(new DictionaryStorage());
    }

    void Dictionary::detach()
    {
      this->ensureStorage();
      if (this->storage.useCount() == 1)
        return;
      DictionaryStorage *copy = new DictionaryStorage();
      copy->entries = this->storage->entries;
      this->storage = Managed<DictionaryStorage>::Wrap(copy);
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

    Managed<DictionaryStorage> Dictionary::getHandle() const
    {
      return this->storage;
    }

  } // namespace core
} // namespace loka
