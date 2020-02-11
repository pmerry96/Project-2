//
// Created by xizhouf on 2/6/20.
//
// Test that string functions work correctly,

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define N  100
#define STR_BUF_SIZE 128

void
print(const char *s)
{
    write(1, s, strlen(s));
}

void
snprintftest(void)
{
    char sbuf[STR_BUF_SIZE + 1];
    char *buf = &sbuf[0];
    int size = STR_BUF_SIZE;

    print("snprintf test\n");
    int n = 5;
    snprintf(buf, size, "%d %c %p %x", n, 'a', &n, n);
    printf("expected: [%d %c %p %x]\tgot:[%s]\n", n, 'a', &n, n, buf);
    if (strcmp("5 a 0x0000000000002F34 5", buf) != 0) {
        print("str test failed\n");
        exit(1);
    }

    print("str test OK\n");
}

int
main(void)
{
    snprintftest();
    exit(0);
}
