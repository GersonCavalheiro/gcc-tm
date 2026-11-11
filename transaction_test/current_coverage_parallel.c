#include <assert.h>
unsigned unrelated __attribute__((used)) = 9;
int unrelated_array[2] __attribute__((used));
int main (void)
{
  int counter = 0;
#pragma omp parallel num_threads(2) shared(counter)
  {
#pragma omp transaction(counter)
    { ++counter; }
  }
  assert (counter == 2 && unrelated == 9);
  return 0;
}
