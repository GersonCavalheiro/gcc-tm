/* Both the original nested controller and its borrowed clone are inspected. */
#include <assert.h>
static int object;
static void leaf (void) { object += 1; }
static void nested (void)
{
#pragma omp transaction(object)
  { leaf (); }
  leaf ();
}
int main (void)
{
  for (int round = 0; round < 5; ++round)
    {
      object = round;
#pragma omp transaction(object)
      { nested (); }
      assert (object == round + 2);
    }
  return 0;
}
