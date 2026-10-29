/* { dg-do compile } */
/* { dg-options "-fopenmp -O2" } */

extern int outside (void);
void f (int x, int y)
{
  const int hidden = 3;
  #pragma omp transaction (x)
  {
    x += y; /* { dg-error "requires explicit registration" } */
    x += hidden; /* { dg-error "requires explicit registration" } */
    x = outside (); /* { dg-error "same-unit prepared definition" } */
  }
}
