// Global allocation operators for Classic (Retro68) builds (#135 item 6).
//
// Every Loka TU compiles with -fno-exceptions, but libstdc++'s prebuilt
// throwing operator new carries the __cxa_throw path -- and with it the
// C++ unwinder (__gxx_personality_v0, execute_cfa_program, search_object)
// -- into every application. Defining the full replacement set here keeps
// the linker from pulling libstdc++'s definitions, so the unwinder
// machinery is never referenced and gc-sections drops it.
//
// Small requests try a chunk, then the single request, then refuse with 0;
// requests above 256 bytes pass straight to NewPtr without a header.
// All six delete forms funnel through scalar delete and the pool release door.
// Contract:
// - Plain new/new[] never return 0: allocation failure aborts via the
//   repository's fatal-abort convention (std::abort, as in LokaAlloc).
//   This replaces the previous throw-crash OOM path with a direct abort.
// - Nothrow new/new[] return 0 on failure, preserving the allocation
//   gate's "backend already gave up" contract (#132): the default
//   LokaAlloc backend and the Window/Scene creation paths probe with
//   new (std::nothrow) and convert 0 into their own failure unit.
//
// This TU is only compiled into LokaAppleToolboxCore (Classic targets);
// host and test builds keep the toolchain-default operators.

#include "core/SmallObjectPool.hpp"
#include "ToolboxSmallObjectPool.hpp"
#include <MacMemory.h>
#include <cstdlib>
#include <new>
#include <string>

namespace
{
  /** Memory Manager storage for every target linking this Classic rail. */
  struct ClassicMemorySource
  {
    // The target's maximum fundamental alignment, which NewPtr meets or
    // exceeds: 8 on PowerPC/Carbon (a double or long long needs 8, and the
    // Memory Manager aligns to at least that), 4 on 68K (where the m68k ABI's
    // strictest fundamental type is 4-aligned and NewPtr returns 4-aligned
    // blocks). Every size class is a multiple of 8, so each slot inherits the
    // chunk base's alignment and stays >= this promise. Large/direct/fallback
    // requests bypass the pool and keep NewPtr's own alignment, unchanged from
    // the pre-pool operator new.
#if defined(__ppc__) || defined(__POWERPC__)
    enum { kAlignment = 8 };
#else
    enum { kAlignment = 4 };
#endif

    static void *acquire(std::size_t size)
    {
      return NewPtr(size);
    }

    static void release(void *storage)
    {
      DisposePtr(static_cast<Ptr>(storage));
    }
  };

  // POD in zero-initialized BSS: usable before main and after static teardown.
  // Chunks belong to the process and are never returned to the zone.
  static loka::core::SmallObjectPool<ClassicMemorySource> gPool;
} // namespace

#if LOKA_RETRO68_DIAGNOSTICS
void LokaClassicPoolReport(loka::core::SmallObjectPoolReport &out)
{
  gPool.report(out);
}
#endif

void *operator new(std::size_t size)
{
  void *storage = gPool.allocate(size);
  if (!storage)
  {
    std::abort();
  }
  return storage;
}

void *operator new[](std::size_t size)
{
  return operator new(size);
}

void *operator new(std::size_t size, const std::nothrow_t &) throw()
{
  return gPool.allocate(size);
}

void *operator new[](std::size_t size, const std::nothrow_t &nothrowTag) throw()
{
  return operator new(size, nothrowTag);
}

void operator delete(void *storage) throw()
{
  if (!storage) return;
  gPool.release(storage);
}

void operator delete[](void *storage) throw()
{
  operator delete(storage);
}

// Sized forms: the prebuilt libstdc++ objects reference these.
void operator delete(void *storage, std::size_t) throw()
{
  operator delete(storage);
}

void operator delete[](void *storage, std::size_t) throw()
{
  operator delete(storage);
}

void operator delete(void *storage, const std::nothrow_t &) throw()
{
  operator delete(storage);
}

void operator delete[](void *storage, const std::nothrow_t &) throw()
{
  operator delete(storage);
}

// libstdc++'s functexcept.o is the other __cxa_throw carrier: container
// code inlined from headers (std::vector growth checks, std::string) calls
// std::__throw_length_error and friends, and the prebuilt definitions
// throw. Satisfying every referenced __throw_* helper here keeps that
// object file out of the link entirely; misuse aborts instead. The set
// must cover everything the applications reference, or the linker would
// pull functexcept.o and collide with these definitions.

#include <bits/functexcept.h>

namespace std
{
  void __throw_bad_alloc()
  {
    abort();
  }

  void __throw_bad_array_new_length()
  {
    abort();
  }

  void __throw_logic_error(const char *)
  {
    abort();
  }

  void __throw_length_error(const char *)
  {
    abort();
  }

  void __throw_out_of_range(const char *)
  {
    abort();
  }

  void __throw_out_of_range_fmt(const char *, ...)
  {
    abort();
  }
} // namespace std

// libstdc++'s guard.o (function-local static init guards) is the final
// __cxa_throw carrier. The Classic application layer is single-threaded,
// so the guard ABI reduces to a byte flag; providing it here keeps
// guard.o out of the link. Covers guard references from prebuilt
// libstdc++ members as well, which -fno-threadsafe-statics on our own
// TUs alone would not.

extern "C"
{
  int __cxa_guard_acquire(long long *guardObject)
  {
    return *reinterpret_cast<char *>(guardObject) == 0;
  }

  void __cxa_guard_release(long long *guardObject)
  {
    *reinterpret_cast<char *>(guardObject) = 1;
  }

  void __cxa_guard_abort(long long *)
  {
  }
}

// The last __gxx_personality_v0 carrier is libstdc++'s string-inst.o: the
// prebuilt std::string explicit instantiation is compiled with exceptions,
// so its members carry landing-pad tables that reference the personality
// routine. Instantiating std::string in this -fno-exceptions TU provides
// those members without any unwind references, keeping string-inst.o out
// of the link so the unwinder and personality routine gc away.
template class std::basic_string<char>;
