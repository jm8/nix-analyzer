#pragma once
#include <nix/config.h>
#include <vector>

#define GC_DEBUG

#if HAVE_BOEHMGC
#define GC_INCLUDE_NEW
#include <gc/gc_allocator.h>
#include <gc/gc_cpp.h>

template <typename T>
using TraceableVector = std::vector<T, traceable_allocator<T>>;
#else
template <typename T>
using TraceableVector = std::vector<T>;
#endif
