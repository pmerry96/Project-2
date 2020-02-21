//
// Created by Philip on 2/19/2020.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> //maybe dont include this
#include <sys/wait.h>

void primeSieve(int, int);

/*
int multiplesof(int* start_At, int up_to, int mulsof)
{
    int new = mulsof + start_At;
    if(new > up_to)
    {
        return start_At;
    }else{
        return new
    }
}

int inarray(int isin, int* vec, int vecsize)
{
    for(int i = 0; i < vecsize; i++)
    {
        if(vec[i] == isin)
            return 1;
    }
    return 0;
}
*/
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
    /*
    if (argc < 2) {
        printf("Error: too few arguments\n");
        exit(0);
    } else {
        int up_to = atoi(argv[1]);
        if (up_to < 2) {
            printf("Error: you must pick a number greater than 2\n");
            exit(0);
        } else {
            int pid;
            int mulsof = 0;
            for (int i = 0; i < up_to; i++) {
                pid = fork();
                if (!pid) {
                    mulsof = i;
                    continue;
                } else {

                }
            }
            int mulsvec[up_to];
            int primesvec[up_to];
            for (int k = 0; k < up_to; k++) {
                primesvec[k] = 0;
            }
            int p = 0;
            if (pid == 0) {
                //children produce multiples

                int start_at = 0;
                for (int i = 0; i < up_to; i++) {
                    mulsvec[i] = multiplesof(&start_at, up_to, mulsof)
                }
            } else {
                //parents push through the sieve
                for (int i = 0; i < up_to;) {
                    if (inarray(i, mulvec, up_to)) {
                        i++;
                    } else {
                        primesvec[p++] = i++;
                    }
                }
            }
            for (int i = 0; i < up_to; i++) {
                if (primesvec[i] != 0)
                    printf("prime n = %d\n", primesvec[i]);
            }
        }
    }
    exit(0);
     */
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