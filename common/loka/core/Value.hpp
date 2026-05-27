#ifndef LOKA_CORE_VALUE_HPP
#define LOKA_CORE_VALUE_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "loka/core/Managed.hpp"
#include "loka/core/String.hpp"

namespace loka
{
  namespace core
  {
    class Array;
    class Dictionary;
    struct ArrayStorage;
    struct DictionaryStorage;

    enum ValueType
    {
      ValueTypeNull = 0,
      ValueTypeBool,
      ValueTypeInt,
      ValueTypeDouble,
      ValueTypeString,
      ValueTypeArray,
      ValueTypeDictionary
    };

    class Value
    {
    public:
      Value();
      explicit Value(bool b);
      explicit Value(long n);
      explicit Value(double d);
      explicit Value(const String &s);
      explicit Value(const Array &array);
      explicit Value(const Dictionary &dictionary);
      Value(const Value &other);
      Value &operator=(const Value &other);
      ~Value();

      static const Value &Null();
      static const Value &True();
      static const Value &False();

      ValueType type() const;
      bool isNull() const;

      bool asBool(bool defaultValue) const;
      long asInt(long defaultValue) const;
      double asDouble(double defaultValue) const;
      const String &asString() const;
      Array asArray() const;
      Dictionary asDictionary() const;

    private:
      ValueType valueType;
      union Storage
      {
        bool boolValue;
        long intValue;
        double doubleValue;
        char stringValue[sizeof(String)];
        char arrayValue[sizeof(loka::core::Managed<ArrayStorage>)];
        char dictionaryValue[sizeof(loka::core::Managed<DictionaryStorage>)];
      } storage;

      // TODO: Investigate pooling for heavier payload slots if profiling shows gains.
      // TODO: Consider light wrapper types around Array/Dictionary to enforce typed views without changing the core
      // Value container.
      // TODO: std::vector works for now; abstract the storage layer only if legacy toolchains require a custom
      // container.

      String *stringSlot();
      const String *stringSlot() const;
      loka::core::Managed<ArrayStorage> *arraySlot();
      const loka::core::Managed<ArrayStorage> *arraySlot() const;
      loka::core::Managed<DictionaryStorage> *dictionarySlot();
      const loka::core::Managed<DictionaryStorage> *dictionarySlot() const;
      void destroyActive();
      void copyFrom(const Value &other);
      friend class Array;
      friend class Dictionary;
    };

    struct ArrayStorage
    {
      ArrayStorage();
      std::vector<Value> values;
    };

    class Array
    {
    public:
      Array();
      Array(const Array &other);
      Array &operator=(const Array &other);
      ~Array();

      std::size_t size() const;
      bool empty() const;
      void clear();
      void pushBack(const Value &value);
      const Value &at(std::size_t index) const;
      Value &at(std::size_t index);
      Value &operator[](std::size_t index);
      const Value &operator[](std::size_t index) const;

    private:
      loka::core::Managed<ArrayStorage> storage;
      explicit Array(const loka::core::Managed<ArrayStorage> &handle);
      void ensureStorage();
      void detach();
      loka::core::Managed<ArrayStorage> getHandle() const;
      friend class Value;
    };

    struct DictionaryEntry
    {
      String logicalKey;
      std::string utf8Key;
      Value value;
    };

    struct DictionaryStorage
    {
      DictionaryStorage();
      std::vector<DictionaryEntry> entries;
    };

    class Dictionary
    {
    public:
      Dictionary();
      Dictionary(const Dictionary &other);
      Dictionary &operator=(const Dictionary &other);
      ~Dictionary();

      std::size_t size() const;
      bool empty() const;
      void clear();
      bool hasKey(const String &key) const;
      Value *find(const String &key);
      const Value *find(const String &key) const;
      void set(const String &key, const Value &value);
      bool remove(const String &key);

    private:
      loka::core::Managed<DictionaryStorage> storage;
      explicit Dictionary(const loka::core::Managed<DictionaryStorage> &handle);
      void ensureStorage();
      void detach();
      std::string normalizeKey(const String &key) const;
      DictionaryEntry *findEntry(const std::string &utf8Key);
      const DictionaryEntry *findEntry(const std::string &utf8Key) const;
      loka::core::Managed<DictionaryStorage> getHandle() const;
      friend class Value;
    };

  } // namespace core
} // namespace loka

#endif // LOKA_CORE_VALUE_HPP
