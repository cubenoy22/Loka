#ifndef LOKA_TOOLBOX_SMALL_OBJECT_POOL_HPP
#define LOKA_TOOLBOX_SMALL_OBJECT_POOL_HPP

#if LOKA_RETRO68_DIAGNOSTICS
#include "core/SmallObjectPool.hpp"

/** Allocation-free process-pool snapshot; inspect out.valid even at teardown. */
void LokaClassicPoolReport(loka::core::SmallObjectPoolReport &out);
#endif

#endif // LOKA_TOOLBOX_SMALL_OBJECT_POOL_HPP
