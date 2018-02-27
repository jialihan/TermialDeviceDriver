#include <stdio.h>
#include <stdlib.h>
/* prototypes */
void fillstructure(void);
void printstructure(void);
/* constants */
/* variables */
struct thing {
  char name[32];
  int age;
  };
typedef struct thing human;

#ifndef _MYEXT_H
#define _MYEXT_H
static const int myx = 245;
static const unsigned long int myy = 45678;
static const double myz = 3.14;
#endif

#ifndef MYFUNC_H
#define MYFUNC_H
void myfunc(void);
#endif



/*
include "myext.h"
#include "myfunc.h"
#include <stdio.h>

int main()
{
    printf("%d\t%lu\t%f\n", myx, myy, myz);
    myfunc();
    return 0;
}
*/