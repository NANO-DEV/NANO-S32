// User program: Test

#include "types.h"
#include "ulib/ulib.h"


// Program entry point
int main(int argc, char* argv[])
{
  putstr("Test program\nargc=%d ", argc);

  for(int i=0; i<argc; i++) {
    putstr("argv[%d]=%s ", i, argv[i]);
  }

  putstr("\n");
  
  return 0;
}
