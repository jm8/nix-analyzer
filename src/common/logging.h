#include <nix/util.hh>
#include <iostream>
#include "common/stringify.h"

#define REPORT_ERROR(e)                                                     \
    (std::cerr << "caught error: " << nix::filterANSIEscapes(e.msg(), true) \
               << " " << __FILE__ << ":" << __LINE__ << "\n")