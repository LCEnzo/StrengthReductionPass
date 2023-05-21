#include <stdio.h>

/* output: 1  4  7  10  13  16  19  22  25  28 */
/* 
 * j => <i, 3, 1>
 * van petlje novo j = 1, i u petlji se uvecava za 3
 */

int main(int argc, const char** argv) {

    int a[30] = {0};
    int j;

    putchar('\n');
    for (int i = 0; i < 10; i = i + 1) {
      j = 3 * i + 1;
      //a[j] = a[j] - 2;
      //i = i + 2;
      printf("%d  ", j);
    }
    putchar('\n');
    putchar('\n');

    return 0;
}
