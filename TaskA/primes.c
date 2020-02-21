//
// Created by Philip on 2/19/2020.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> //maybe dont include this
#include <sys/wait.h>

/*
 * Statements about part A of project 2
 *  I have been primarily focused on Part B, as that is where the majority of the points lie.
 *  As the due date draws near, I have decided to put part A on the back-burner in favor of
 *  Part B. As such, the code runs and initiates the sequence for a prime sieve, but hangs after the
 *  first read(...) (inside primeSieve()). If you are seeing this comment it means I did not have
 *  time to complete task A before the due date and I still intend to return and finish part A.
 */

void primeSieve(int, int);

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Error: too few arguments\n");
        printf("./primes <get primes below this integer value>\n");
        exit(0);
    }else{
        int primes_up_to = atoi(argv[1]);
        int p[2];
        pipe(p);
        int two = 2;
        int pid = fork();
        if(pid == 0) {
        	//child will just write
	        write(/*p[1]*/1, &two, sizeof(two));
	        close(p[0]);
	        close(p[1]);
	        printf("first call child done\n");
        }else{
        	//parent will call the sieve
	        wait(0);
	        printf("first call bout to enter sieve\n");
	        primeSieve(primes_up_to, pid);
	        printf("first call parent done\n");
	        close(p[0]);
	        close(p[1]);
        }
    }
}

int isprime(int num)
{
    if(num == 2)
    {
        return 1;
    }
    for(int i = 2; i < num; i++)
    {
        if(num % i)
        {
           return 0;
        }
    }
    return 1;
}

void primeSieve(int up_to, int pid)
{
	printf("calling sieve on pid = %d\n", pid);
    int p[2];
    pipe(p);
    int n = 5;
    printf("preread\n");
    int incoming = read(p[0] , &n, sizeof(n));
    printf("postread\n");
    if(incoming < 0)
    {
    	printf("read Error in Primeseive, coming from PID = %d\n", pid);
    }
    if(isprime(n))
        printf("pid=%d prime %d\n", pid, n);
    pid = fork();
    if(pid == 0)
    {
        n++;
        write(p[1], &n, sizeof(n));
        close(p[0]);
        close(p[1]);
    }else{
	    primeSieve(up_to, pid);
        wait(0);
	    close(p[0]);
	    close(p[1]);
    }
	close(p[0]);
	close(p[1]);
}