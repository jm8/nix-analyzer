#include <nix/config.h>

#if HAVE_BOEHMGC
#define GC_INCLUDE_NEW
#include <gc/gc_allocator.h>
#include <gc/gc_cpp.h>
#endif
