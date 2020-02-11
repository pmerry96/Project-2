//
// Created by xizhouf on 2/6/20.
//
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/tsh.h"

#define ERR_STR_BUF_SIZE 255
static  char err_str_buf[ERR_STR_BUF_SIZE + 1];

void
tsh_error(const char* prefix, const char *cause, const char *filename,  int lineno)
{
    char *buf = &err_str_buf[0];
    int size = ERR_STR_BUF_SIZE;
    snprintf(buf, size, "%s: %s: File=%s Line=%d \n", prefix, cause, filename, lineno);
    write(2, buf, strlen(buf));
}

void
tsh_debug(const char *filename,  int lineno, const char *fmt, ...) {
    char *buf = &err_str_buf[0];
    int size = ERR_STR_BUF_SIZE;
    int nc;

    // print prefix
    nc = snprintf(buf, size, "Debug: ");
    buf += nc;
    size -= nc;

    // print message
    va_list ap;
    va_start(ap, fmt);
    nc = vsnprintf(buf, size, fmt, ap);
    if (nc >= 0 && nc <= size)
        buf[nc] = '\0';
    buf += nc;
    size -= nc;

    // print location
    nc = snprintf(buf, size, ": File=%s Line=%d \n", filename, lineno);
    buf[nc] = '\0';

    // write to stdout
    buf = &err_str_buf[0];
    write(2, buf, strlen(buf));
}