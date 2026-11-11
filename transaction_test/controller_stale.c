/* A removed first body must not supply executable temporaries to a later one. */
int main (void)
{
  int array[2] = { 1, 2 };
  int *pointer = array;
#pragma omp transaction(array, pointer)
  { pointer = array + 1; }
#pragma omp transaction(array, pointer)
  { *pointer = 9; }
  return array[1];
}
