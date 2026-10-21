/* { dg-do compile } */
/* { dg-options "-fopenmp" } */

class base
{
  static int hidden; /* { dg-message "private" } */
};

class derived : public base
{
  void test ()
  {
    #pragma omp transaction (hidden) /* { dg-error "private" } */
    {}
  }
};

struct left { static int item; }; /* { dg-message "item" } */
struct right { static int item; }; /* { dg-message "item" } */
struct ambiguous : left, right
{
  void test ()
  {
    #pragma omp transaction (item) /* { dg-error "ambiguous" } */
    {}
  }
};

