#include <stdio.h>

int main() 
{
    int a = 1234567;
    int b, c, d;

    for (int i = 0; i < 20*1000*1000; i++)
    {
        b = a / 16;

        c = b / 8;

        d = c * 128;
    }

    return 0;
}
