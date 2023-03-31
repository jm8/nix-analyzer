#include <iostream>
#include "common/stringify.h"
#define REPORT_ERROR(e)                                                 \
    (std::cerr << "caught error: " << e.msg() << " " << __FILE__ << ":" \
               << __LINE__ << "\n")