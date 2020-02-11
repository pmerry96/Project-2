//
// Created by xizhouf on 2/6/20.
// Test the error functions
//
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/tsh.h"

//#define STR_BUF_SIZE 255
//
//#define Error(msg)  tsh_error((msg), __FILE__, __LINE__)
//
void
print(const char *s)
{
    write(1, s, strlen(s));
}

//void
//tsh_error(const char *cause, const char *filename,  int lineno)
//{
//    static  char sbuf[STR_BUF_SIZE + 1];
//    char *buf = &sbuf[0];
//    int size = STR_BUF_SIZE;
//    snprintf(buf, size, "error: %s: File=%s Line=%d \n", cause, filename, lineno);
//    write(1, buf, strlen(buf));
//}

void
error_test(void)
{
    ErrorU("just for testing");
    ErrorP("function not implemented yet");
    ErrorS("memory allocation");
    Debug("error_test");
    Debug("v=%d", 100);

    print("error test OK\n");
}

int
main(void)
{
    error_test();
    exit(0);
}