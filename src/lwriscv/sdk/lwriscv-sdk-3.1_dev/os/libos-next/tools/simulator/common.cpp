#include <stdio.h>
#include <stdlib.h>

void test_failed(const char *reason)
{
    printf(
        R"(
Test failed: %s

To debug this test:
  cd _out
  ./test_driver --gdb

If you wish to search a highlighted instruction trace:
  ./test_driver --trace | less -r

Or alternatively, you can use both
  ./test_driver --gdb --trace

)",
        reason);
    exit(1);
}
