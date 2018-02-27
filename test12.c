/*
 * Test for concurrent writing 
 */

#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <terminals.h>

void writer1(void *);

char string1[] = "abcdefghijklmnopqrstuvwxyz\n";
int length1 = sizeof(string1) - 1;


int
main(int argc, char **argv)
{
    InitTerminalDriver();
    InitTerminal(1);

    if (argc > 1) HardwareOutputSpeed(1, atoi(argv[1]));
    if (argc > 2) HardwareInputSpeed(1, atoi(argv[2]));

    ThreadCreate(writer1, NULL);

    ThreadWaitAll();

    exit(0);
}

void
writer1(void *arg)
{
    int status;

    status = WriteTerminal(1, string1, length1);
    if (status != length1){
        fprintf(stderr, "Error: writer status = %d, len = %d\n",
        status, length1);
    }

    while (1)
        ;
}

