#include <stdio.h>

int addFun(int a, int b) {
    int ret;
    ret = a + b;
    return ret;
}

int main() {
    printf("%d + %d = %d\n", 1, 2, addFun(2, 3));
    return 0;
}