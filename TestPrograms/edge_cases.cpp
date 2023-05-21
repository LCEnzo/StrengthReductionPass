#include <limits.h>
#include <stdio.h>
#include <stdbool.h>

int main() {
    int a = -7;
    int b = a * 64;                             // Should be optimized to a left shift
    int b2 = 64 * a;
    printf("%b", b2 == b);

    int c = INT_MAX;
    int d = c * 2;                              // Should result in an overflow
    double e = __DBL_MAX__;
    double f = e * 2;
    int mod = 3333 % 4;

    printf("a: %d\n", a);
    printf("b: %d\n", b);
    printf("c: %d\n", c);
    printf("d: %d\n", d);
    printf("e: %f\n", e);
    printf("f: %f\n", f);

    printf("15 / 2 = %d\n\n", 15 / 2);

    unsigned int ua = 7;
    unsigned int ub = 64 * ua;
    unsigned int ub2 = ua * 64;
    printf("%b", ub2 == ub);
    printf("ua: %u\n", ua);
    printf("ub: %u\n", ub);

    int foo = -28;
    int bar = foo / 4;
    printf("%d\n", bar);

    unsigned int ufoo = 28;
    unsigned int ubar = ufoo / 4;
    printf("%u\n", ubar);

    printf("Modulo [0, 10000) %% 4:\n");
    for (int i = 0; i < 10000; i++) {
        mod = i % 4;
        printf("%d ", mod);
    } printf("\n");

    printf("Modulo [0, -10000) %% 4: \n");
    for (int i = 0; i < -10000; i--) {
        mod = i % 4;
        printf("%d ", mod);
    } printf("\n");

    return 0;
}