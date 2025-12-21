#ifndef LOKA_CORE_VALUE_HPP
#define LOKA_CORE_VALUE_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "core/Managed.hpp"
#include "loka/core/String.hpp"

namespace loka
{
  namespace core
  {
    class Array;
    class Dictionary;

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

      static Value Null();

      ValueType type() const;
      bool isNull() const;

      bool asBool(bool defaultValue) const;
      long asInt(long defaultValue) const;
      double asDouble(double defaultValue) const;
      const String &asString() const;
      Array asArray() const;
      Dictionary asDictionary() const;

      void setBool(bool b);
      void setInt(long n);
      void setDouble(double d);
      void setString(const String &s);
      void setArray(const Array &array);
      void setDictionary(const Dictionary &dictionary);

    private:
      ValueType valueType;
      bool boolValue;
      long intValue;
      double doubleValue;
      String stringValue;
      Managed<struct ArrayStorage> arrayStorage;
      Managed<struct DictionaryStorage> dictionaryStorage;

      void reset();
      void releaseRefs();
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
      Managed<ArrayStorage> storage;
      explicit Array(const Managed<ArrayStorage> &handle);
      void ensureStorage();
      void detach();
      Managed<ArrayStorage> getHandle() const;
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
      Managed<DictionaryStorage> storage;
      explicit Dictionary(const Managed<DictionaryStorage> &handle);
      void ensureStorage();
      void detach();
      std::string normalizeKey(const String &key) const;
      DictionaryEntry *findEntry(const std::string &utf8Key);
      const DictionaryEntry *findEntry(const std::string &utf8Key) const;
      Managed<DictionaryStorage> getHandle() const;
      friend class Value;
    };

  } // namespace core
} // namespace loka

#endif // LOKA_CORE_VALUE_HPP
