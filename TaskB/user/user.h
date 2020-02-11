#ifndef __USER_H
#define __USER_H
//#include <stdarg.h>

struct stat;
struct rtcdate;

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
int ntas();
int crash(const char*, int);
int mount(char*, char *);
int umount(char*);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
int snprintf(char *str, int size, const char *format, ...);
void fprintf(int, const char*, ...);
void printf(const char*, ...);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);

//// tsh_util.c
//#define ErrorU(cause)  tsh_error("An error has occurred", (cause), __FILE__, __LINE__)
//#define ErrorP(cause)  tsh_error("program error", (cause), __FILE__, __LINE__)
//#define ErrorS(cause)  tsh_error("system error", (cause), __FILE__, __LINE__)
//#define Debug(msg_, ...)   tsh_debug(__FILE__, __LINE__, (msg_), ##__VA_ARGS__)
//void tsh_error(const char *prefix, const char *cause, const char *filename,  int lineno);
//void tsh_debug(const char *filename,  int lineno, const char *fmt, ...);

#endif