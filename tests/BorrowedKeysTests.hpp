#ifndef LOKA_TESTS_BORROWED_KEYS_TESTS_HPP
#define LOKA_TESTS_BORROWED_KEYS_TESTS_HPP

#include "app/scene/BorrowedKeys.hpp"
#include "support/TestVerify.hpp"

inline void testBorrowedKeysAddressContract()
{
  typedef loka::app::scene::BorrowedKeys<3> Keys;
  int first = 7;
  int second = 7;
  const void *sources[] = {0, &first, &second};
  Keys values[27];

  // Every combination includes null in each position, equal-valued objects
  // at distinct addresses, shared prefixes, and reversed suffixes.
  for (std::size_t i = 0; i < 27; ++i)
  {
    LOKA_VERIFY(!values[i].complete());
    for (std::size_t key = 0; key < 3; ++key)
      LOKA_VERIFY(values[i].get(key) == 0);
    values[i].set(0, sources[i / 9]);
    values[i].set(1, sources[(i / 3) % 3]);
    values[i].set(2, sources[i % 3]);
    LOKA_VERIFY(values[i].get(0) == sources[i / 9]);
    LOKA_VERIFY(values[i].get(1) == sources[(i / 3) % 3]);
    LOKA_VERIFY(values[i].get(2) == sources[i % 3]);
    const bool allPresent = i / 9 != 0 && (i / 3) % 3 != 0 && i % 3 != 0;
    LOKA_VERIFY(values[i].complete() == allPresent);
    LOKA_VERIFY(!(values[i] < values[i]));
    const Keys copy(values[i]);
    LOKA_VERIFY(copy == values[i]);
    LOKA_VERIFY(!(copy < values[i]) && !(values[i] < copy));
  }

  for (std::size_t i = 0; i < 27; ++i)
    for (std::size_t j = 0; j < 27; ++j)
    {
      LOKA_VERIFY((values[i] == values[j]) == (i == j));
      const bool less = values[i] < values[j];
      const bool greater = values[j] < values[i];
      LOKA_VERIFY(!(less && greater));
      LOKA_VERIFY((!less && !greater) == (values[i] == values[j]));
      // Pin first-differing-key order, not just any strict weak order.
      bool expected = false;
      for (std::size_t key = 0; key < 3; ++key)
        if (values[i].get(key) != values[j].get(key))
        {
          expected = std::less<const void *>()(values[i].get(key), values[j].get(key));
          break;
        }
      LOKA_VERIFY(less == expected);
      for (std::size_t k = 0; k < 27; ++k)
      {
        if (less && values[j] < values[k])
          LOKA_VERIFY(values[i] < values[k]);
        if (!less && !greater && !(values[j] < values[k]) && !(values[k] < values[j]))
          LOKA_VERIFY(!(values[i] < values[k]) && !(values[k] < values[i]));
      }
    }

  Keys copy(values[26]);
  copy.set(2, 0);
  LOKA_VERIFY(!copy.complete());
  LOKA_VERIFY(values[26].complete());
  LOKA_VERIFY(!(copy == values[26]));
  copy = values[26];
  second = 8;
  LOKA_VERIFY(copy == values[26]);
  LOKA_VERIFY(copy.get(2) == &second);
}

#endif // LOKA_TESTS_BORROWED_KEYS_TESTS_HPP
