/* Preserved unused prepared bodies are not fictional active graph roots. */
static int unused (void)
{
  int value = 0;
#pragma omp transaction(value)
  { ++value; }
  return value;
}
int main (void) { return 0; }
