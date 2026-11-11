/* An ordinary call may change a global selector before the controller. */
static int selector, value, observed;
__attribute__((noinline)) static void choose (void) { selector = 0; }
int main (void)
{
  selector = 1;
  choose ();
  if (selector == 0)
    {
#pragma omp transaction(value, observed)
      { ++value; observed = value; }
    }
  value = 2;
  /* Both exits are nonzero: an unchecked END must still be refused before
     ordinary global publication, not mistaken for forwarding its status. */
  return observed == 1 && value == 2 ? 17 : 19;
}
