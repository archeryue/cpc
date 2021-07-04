#include <stdio.h>

int add(int a, int b) {
    int ret;
    ret = a + b;
    return ret;
}

int main() {
    // printf("%d + %d = %d\n", 1, 2, add(1, 2));
    add(1, 2);
    return 0;
}