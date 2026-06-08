#include "../src/thvm.c"
#include "test.h"
int main(void){ thvm_init(); int f=0;
  TEST_BEGIN("kvar/clamp-to-hi");
  u32 s = kvar_alloc("s", 1, 16);
  kvar_set_runtime(s, 9999);                 // absurd bind -> must clamp to 16
  u32 got = kvar_runtime(s);
  printf("  bound 9999 -> runtime %u (want 16)\n", got);
  if (got != 16) f++;
  kvar_set_runtime(s, 5); if (kvar_runtime(s) != 5) f++;   // normal bind unchanged
  printf("  %s (%d failures)\n", f==0?"ok":"FAIL", f); return f==0?0:1; }
