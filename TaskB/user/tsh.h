//
// Created by xizhouf on 2/6/20.
//
#ifndef PROJECT2_TSH_H
#define PROJECT2_TSH_H
#include <stdarg.h>

// tsh limits
#define TSH_MAX_CMD_LINE_LENGTH     255
#define TSH_MAX_NUM_TOKENS          32
#define TSH_MAX_NUM_ARGUMENTS       6
#define TSH_MAX_CMD_LIST_LENGTH     6
#define TSH_MAX_PIPELINE_LENGTH     6
#define TSH_MAX_FILENAME_LENGTH     64

// tsh_util.c
#define ErrorU(cause)  tsh_error("An error has occurred", (cause), __FILE__, __LINE__)
#define ErrorP(cause)  tsh_error("program error", (cause), __FILE__, __LINE__)
#define ErrorS(cause)  tsh_error("system error", (cause), __FILE__, __LINE__)
#define Debug(msg_, ...)   tsh_debug(__FILE__, __LINE__, (msg_), ##__VA_ARGS__)
void tsh_error(const char *prefix, const char *cause, const char *filename,  int lineno);
void tsh_debug(const char *filename,  int lineno, const char *fmt, ...);

// printf.c
int vsnprintf(char *buf, int size, const char *fmt, va_list ap);
#endif //PROJECT2_TSH_H
