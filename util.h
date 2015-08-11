#ifndef HARMONICA_UTIL_H
#define HARMONICA_UTIL_H

#include <chdl/chdl.h>
#include <chdl/ag.h>
#include <chdl/net.h>

namespace chdl {
  // Population count of bit vector; used by performance counters.
  static bvec<1> PopCount(const bvec<1> &x) { return bvec<1>(x[0]); }

  template <unsigned N> static bvec<LOG2(N) + 1> PopCount(const bvec<N> &x) {
    bvec<N/2> a(x[range<0,N/2-1>()]);
    bvec<N - N/2> b(x[range<N/2,N-1>()]);
    return Zext<LOG2(N) + 1>(PopCount(a)) + Zext<LOG2(N) + 1>(PopCount(b));
  } 
}

#endif
