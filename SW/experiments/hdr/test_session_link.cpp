#include "bracket_session.h"
#include <cassert>
#include <cstdio>
int main() {
 HdrSession s;HdrSession_Init(&s);
 assert(s.state==HDR_IDLE);assert(HdrFrame_Validate(nullptr));
 std::printf("PASS C++ consumer/C object linkage; host context bytes=%zu (not R5 footprint)\n",sizeof(s));
}
