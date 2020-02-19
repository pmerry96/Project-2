#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#define HAS_FORKED = 1
#define HAS_NOT_FORKED = 0


struct node{
    int val;
    node* next;
    node* prev;
};

int* get_primes(int);

int main(int argc, char *argv[]) {
    if(argc < 2)
    {
        printf("Too Few Arguments. Please enter the executable AND the number you want primes up to.\n\n");
    }else{
        int* primes_vec = get_primes(argc[1]);
    }
    exit(0);
}

//maybe return nodes for here
int multiples(int start_at, int multiples_of, int up_to, int_node* multiples_vec)
{
    if((start_at + multiples_of) < up_to) {
        int new = start_at + multiples_of
        multiples(new, multiples_of, up_to);
        return(new);
    } else {
        return(0);
    }
}

//maybe dont return node here
struct node get_primes(int up_to)
{
    int has_forked = HAS_NOT_FORKED;
//  int pid = fork();
    if(pid == 0){
        //in the child process we need to take in numbers
        for(int i = 1; i < up_to; i++)
        {
            if(HAS_NOT_FORKED)
            {
                int pid = fork();
                if(pid == 0)
                {
                    struct node primes_node;
                    primes_node.val = 'something';
                    has_forked = HAS_FORKED;
                    multiples()
                }else{
                    has_forked = HAS_FORKED;
                }
            }
        }
    }else{
        // in the parent process we need to generate numbers
    }
}