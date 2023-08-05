#include <stdio.h>

int main(int argc, const char** argv) {

    int a[30] = {0};
    long long int j, i, k, l;

    for (i = 3; i < 10; i = i + 2) {
      j = 4*i + 981; // j => (i, 4, 981)
      //a[j % 30] = sin(5*j);
      printf("j: %5lld    ", j);
      k = 821*i + 99;
      printf("k: %5lld\n", k);
      //a[j % 30] = cos(7*k);
    }
    printf("\n");

    return 0;
}
