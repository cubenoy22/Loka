/**
 * A borrowed State<T>* permits reading through get(), but has no set() door.
 * This refuses twin pins that contract together with
 * accepts_get_on_borrowed_state.cpp, which shares the same header environment.
 */
#include "core/State.hpp"

void probe(loka::core::State<int> *borrowed) { borrowed->set(1); }
