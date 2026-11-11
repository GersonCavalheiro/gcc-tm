/* No lexical incoming channel is currently generated for this global value. */
static int array[2];
static int *pointer;
int main (void)
{
  pointer = array;
#pragma omp transaction(array, pointer)
  { pointer[0] = 1; }
  return 0;
}
