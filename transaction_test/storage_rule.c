#include <assert.h>
int storage_global = 5;
__attribute__((noinline)) int storage_probe (int *p)
{
  int storage_local = 7;
  return storage_local + *p;
}
int main (void) { assert (storage_probe (&storage_global) == 12); return 0; }
