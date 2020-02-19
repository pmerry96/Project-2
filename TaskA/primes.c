//
// Created by Philip on 2/19/2020.
//

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

int main(int argc, char** argv) {
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
}
