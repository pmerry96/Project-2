//
// test program for the shell lab.
// run it in xv6 like this:
// $ testsh nsh
//

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"


#define STDIN  0
#define STDOUT 1
#define STDERR 2
unsigned int seed = 123456789;

// return a random integer.
// from Wikipedia, linear congruential generator, glibc's constants.
unsigned int
rand()
{
  unsigned int a = 1103515245;
  unsigned int c = 12345;
  unsigned int m = (1 << 31);
  seed = (a * seed + c) % m;
  return seed;
}

// generate a random string of the indicated length.
char *
randstring(char *buf, int n)
{
  for(int i = 0; i < n-1; i++)
    buf[i] = "abcdefghijklmnopqrstuvwxyz"[rand() % 26];
  buf[n-1] = '\0';
  return buf;
}

// create a file with the indicated content.
void
writefile(char *name, char *data)
{
  unlink(name); // since no truncation
  int fd = open(name, O_CREATE|O_WRONLY);
  if(fd < 0){
    fprintf(STDERR, "testsh: could not write %s\n", name);
    exit(-1);
  }
  if(write(fd, data, strlen(data)) != strlen(data)){
    fprintf(STDERR, "testsh: write failed\n");
    exit(-1);
  }
  close(fd);
}

// return the content of a file.
void
readfile(char *name, char *data, int max)
{
  data[0] = '\0';
  int fd = open(name, 0);
  if(fd < 0){
    fprintf(STDERR, "testsh: open %s failed\n", name);
    return;
  }
  int n = read(fd, data, max-1);
  close(fd);
  if(n < 0){
    fprintf(STDERR, "testsh: read %s failed\n", name);
    return;
  }
  data[n] = '\0';
}

// look for the small string in the big string;
// return the address in the big string, or 0.
char *
strstr(char *big, char *small)
{
  if(small[0] == '\0')
    return big;
  for(int i = 0; big[i]; i++){
    int j;
    for(j = 0; small[j]; j++){
      if(big[i+j] != small[j]){
        break;
      }
    }
    if(small[j] == '\0'){
      return big + i;
    }
  }
  return 0;
}

// argv[1] -- the shell to be tested.
char *shname;

// fire up the shell to be tested, send it cmd on
// its input, collect the output, check that the
// output includes the expect argument.
// if tight = 1, don't allow much extraneous output.
int
one(char *cmd, char *expect, int tight)
{
  char infile[12], outfile[12];

  randstring(infile, sizeof(infile));
  randstring(outfile, sizeof(outfile));

  writefile(infile, cmd);
  unlink(outfile);

  int pid = fork();
  if(pid < 0){
    fprintf(STDERR, "testsh: fork() failed\n");
    exit(-1);
  }

  if(pid == 0){
    close(0);
    if(open(infile, 0) != 0){
      fprintf(STDERR, "testsh: child open != 0\n");
      exit(-1);
    }
    close(1);
    if(open(outfile, O_CREATE|O_WRONLY) != 1){
      fprintf(STDERR, "testsh: child open != 1\n");
      exit(-1);
    }
    char *argv[2];
    argv[0] = shname;
    argv[1] = 0;
    exec(shname, argv);
    fprintf(STDERR, "testsh: exec %s failed\n", shname);
    exit(-1);
  }

  if(wait(0) != pid){
    fprintf(STDERR, "testsh: unexpected wait() return\n");
    exit(-1);
  }
  unlink(infile);

  char out[256];
  readfile(outfile, out, sizeof(out));
  unlink(outfile);

  if(strstr(out, expect) != 0){
    if(tight && strlen(out) > strlen(expect) + 20){
      fprintf(STDERR, "testsh: saw expected output, but too much else as well\n");
      return 0; // fail
    }
    return 1; // pass
  }
  return 0; // fail
}


// test a command with arguments.
void
t1(int *ok)
{
  printf("simple echo: ");
  if(one("echo hello goodbye\n", "hello goodbye", 1) == 0){
    printf("FAIL\n");
    *ok = 0;
  } else {
    printf("PASS\n");
  }
}

// test a command with arguments.
void
t2(int *ok)
{
  printf("simple grep: ");
  if(one("grep constitute README\n", "The code in the files that constitute xv6 is", 1) == 0){
    printf("FAIL\n");
    *ok = 0;
  } else {
    printf("PASS\n");
  }
}

// test a command, then a newline, then another command.
void
t3(int *ok)
{
  printf("two commands: ");
  if(one("echo x\necho goodbye\n", "goodbye", 1) == 0){
    printf("FAIL\n");
    *ok = 0;
  } else {
    printf("PASS\n");
  }
}

// test output redirection: echo xxx > file
void
t4(int *ok)
{
  printf("output redirection: ");

  char file[16];
  randstring(file, 12);

  char data[16];
  randstring(data, 12);

  char cmd[64];
  strcpy(cmd, "echo ");
  strcpy(cmd+strlen(cmd), data);
  strcpy(cmd+strlen(cmd), " > ");
  strcpy(cmd+strlen(cmd), file);
  strcpy(cmd+strlen(cmd), "\n");

  if(one(cmd, "", 1) == 0){
    printf("FAIL\n");
    *ok = 0;
  } else {
    char buf[64];
    readfile(file, buf, sizeof(buf));
    if(strstr(buf, data) == 0){
      printf("FAIL\n");
      *ok = 0;
    } else {
      printf("PASS\n");
    }
  }

  unlink(file);
}

// test input redirection: cat < file
void
t5(int *ok)
{
  printf("input redirection: ");

  char file[32];
  randstring(file, 12);

  char data[32];
  randstring(data, 12);
  writefile(file, data);

  char cmd[32];
  strcpy(cmd, "cat < ");
  strcpy(cmd+strlen(cmd), file);
  strcpy(cmd+strlen(cmd), "\n");

  if(one(cmd, data, 1) == 0){
    printf("FAIL\n");
    *ok = 0;
  } else {
    printf("PASS\n");
  }

  unlink(file);
}

// test a command with both input and output redirection.
void
t6(int *ok)
{
  printf("both redirections: ");
  unlink("testsh.out");
  if(one("grep pointers < README > testsh.out\n", "", 1) == 0){
    printf("FAIL\n");
    *ok = 0;
  } else {
    char buf[64];
    readfile("testsh.out", buf, sizeof(buf));
    if(strstr(buf, "provides pointers to on-line resources") == 0){
      printf("FAIL\n");
      *ok = 0;
    } else {
      printf("PASS\n");
    }
  }
  unlink("testsh.out");
}

// test a pipe with cat filename | cat.
void
t7(int *ok)
{
  printf("simple pipe: ");

  char name[32], data[32];
  randstring(name, 12);
  randstring(data, 12);
  writefile(name, data);

  char cmd[64];
  strcpy(cmd, "cat ");
  strcpy(cmd + strlen(cmd), name);
  strcpy(cmd + strlen(cmd), " | cat\n");
  
  if(one(cmd, data, 1) == 0){
    printf("FAIL\n");
    *ok = 0;
  } else {
    printf("PASS\n");
  }

  unlink(name);
}

// test a pipe with cat README | cat | cat | cat.
void
t7_1(int *ok)
{
    printf("a four-element pipe: ");

    char name[32], data[32];
    randstring(name, 12);
    randstring(data, 12);
    writefile(name, data);

    char cmd[64];
    strcpy(cmd, "cat ");
    strcpy(cmd + strlen(cmd), name);
    strcpy(cmd + strlen(cmd), " | cat | cat | cat\n");

    if(one(cmd, data, 1) == 0){
        printf("FAIL\n");
        *ok = 0;
    } else {
        printf("PASS\n");
    }

    unlink(name);
}


// test a pipeline that has both redirection and a pipe.
void
t8(int *ok)
{
  printf("pipe and redirects: ");
  
  if(one("grep suggestions < README | wc > testsh.out\n", "", 1) == 0){
    printf("FAIL\n");
    *ok = 0;
  } else {
    char buf[64];
    readfile("testsh.out", buf, sizeof(buf));
    if(strstr(buf, "1 11 71") == 0){
      printf("FAIL\n");
      *ok = 0;
    } else {
      printf("PASS\n");
    }
  }

  unlink("testsh.out");
}

// ask the shell to execute many commands, to check
// if it leaks file descriptors.
void
t9(int *ok)
{
  printf("lots of commands: ");

  char term[32];
  randstring(term, 12);
  
  char *cmd = malloc(25 * 36 + 100);
  if(cmd == 0){
    fprintf(STDERR, "testsh: malloc failed\n");
    exit(-1);
  }

  cmd[0] = '\0';
  for(int i = 0; i < 17+(rand()%6); i++){
    strcpy(cmd + strlen(cmd), "echo x < README > tso\n");
    strcpy(cmd + strlen(cmd), "echo x | echo\n");
  }
  strcpy(cmd + strlen(cmd), "echo ");
  strcpy(cmd + strlen(cmd), term);
  strcpy(cmd + strlen(cmd), " > tso\n");
  strcpy(cmd + strlen(cmd), "cat < tso\n");

  if(one(cmd, term, 0) == 0){
    printf("FAIL\n");
    *ok = 0;
  } else {
    printf("PASS\n");
  }

  unlink("tso");
  free(cmd);
}

int
main(int argc, char *argv[])
{
  if(argc != 2){
    fprintf(STDERR, "Usage: testsh nsh\n");
    exit(-1);
  }
  shname = argv[1];
  
  seed += getpid();

  int ok = 1;

  t1(&ok);
  t2(&ok);
  t3(&ok);
  t4(&ok);
  t5(&ok);
  t6(&ok);
  t7(&ok);
  t8(&ok);
  t9(&ok);


  if(ok){
    printf("passed all tests\n");
  } else {
    printf("failed some tests\n");
  }
  
  exit(0);
}
