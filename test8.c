/*
 * Test for single terminal reading 
 */

#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <terminals.h>

void writer(void *);


char string1[] = "jjjjjjjjhhhhhhhh\n";
int length1 = sizeof(string1) - 1;


int
main(int argc, char **argv)
{
    InitTerminalDriver();
    int a = InitTerminal(0);
    int b = InitTerminal(1);
    int c = InitTerminal(2);
    int d = InitTerminal(3);

    printf("%d %d %d %d\n", a, b, c, d);
    fflush(stdout);


    if (argc > 1) HardwareOutputSpeed(1, atoi(argv[1]));
    if (argc > 2) HardwareInputSpeed(1, atoi(argv[2]));


    ThreadCreate(writer, NULL);

    ThreadWaitAll();

    exit(0);
}

void
writer(void *arg)
{
    int w_status, r_status;
    char buf1[10];

    w_status = WriteTerminal(1, string1, length1);

    r_status = ReadTerminal(*((int *)arg), buf1, 5000);
    
    printf("WriteTerminal STATUS = %d , ReadTerminal STATUS =  %d\n", w_status, r_status);
    fflush(stdout);
}


