/**
 * A borrowed State<T>* permits reading through get(), but has no set() door.
 * This accepts twin pins that contract together with
 * refuses_set_on_borrowed_state.cpp, which shares the same header environment.
 */
#include "core/State.hpp"

int probe(loka::core::State<int> *borrowed) { return borrowed->get(); }
