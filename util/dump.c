/* 3-out-of-4 Tsunami mode dumping tool */

// Compile:
//   gcc dump.c -o dump
#include <stdio.h>

int main(void)
{
  int c;
  int i=0, j=0;

  while ( EOF!=(c=getc(stdin)) ) {
     printf("%02X ", c);
     i++;
     if(i>=3) {
        j++;
        if (j>=3) { j=0; printf("\n"); }
        else { printf("    "); }
        i = 0;
     }
  }
}
